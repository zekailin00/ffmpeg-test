extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include "encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


Encoder::Encoder(
    int width, int height, int64_t bit_rate,
    AVCodecID codec_id, AVPixelFormat pixelFormat):
    PIX_FMT(pixelFormat)
{
    this->width = width;
    this->height = height;
    frame_index = 0;

    // codec
    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        throw "Codec not found";
    }

    enc_ctx = avcodec_alloc_context3(codec);
    if (!enc_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        throw "Could not allocate video codec context";
    }

    // Set encoding parameters 
    enc_ctx->bit_rate = bit_rate;
    enc_ctx->width = width;
    enc_ctx->height = height;
    enc_ctx->gop_size = 10;
    enc_ctx->max_b_frames = 1;
    enc_ctx->pix_fmt = Encoder::PIX_FMT;

    // FIXME: variable framerate
    enc_ctx->time_base = (AVRational){1, 25};
    enc_ctx->framerate = (AVRational){25, 1};

    // Open the codec
    if (avcodec_open2(enc_ctx, codec, NULL) < 0)
    {
        fprintf(stderr, "Could not open codec\n");
        throw "Could not open codec";
    }

    // Allocate video frame
    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Could not allocate video frame\n");
        throw "Could not allocate video frame";
    }

    frame->format = enc_ctx->pix_fmt;
    frame->width  = enc_ctx->width;
    frame->height = enc_ctx->height;

    sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_RGBA,
        width, height, Encoder::PIX_FMT,
        SWS_BILINEAR, NULL, NULL, NULL
    );
    if (!sws_ctx) {
        fprintf(stderr, "Could not initialize the conversion context\n");
        throw "Could not initialize the conversion context";
    }

    int ret;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        throw "Could not allocate the video frame data";
    }

    pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "Could not allocate packet\n");
        throw "Could not allocate packet";
    }

    output_size = 0;
    output_data = (uint8_t *)malloc(pkt->size);
    if (!output_data)
    {
        fprintf(stderr, "Failed to allocate memory for encoded data\n");
        throw "Failed to allocate memory for encoded data";
    }
}

int Encoder::encode(
    AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt,
    uint8_t **output, int *output_size)
{
    int ret;

    // Send the frame to the encoder
    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending frame to encoder\n");
        return ret;
    }

    *output_size = 0;

    // Read the encoded packets. can be more than one.
    while(ret >= 0)
    {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            printf("Stop here\n");
            return 0;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error during encoding\n");
            return ret;
        }

        *output = (uint8_t *)realloc(*output, *output_size + pkt->size);
        if (!*output) {
            fprintf(stderr, "Failed to allocate memory for encoded data\n");
            av_packet_unref(pkt);
            return -1;
        }
        
        memcpy(*output + *output_size, pkt->data, pkt->size);
        *output_size += pkt->size;
        
        printf("Write packet %3" PRId64 " (size=%5d)\n", pkt->pts, pkt->size);
        av_packet_unref(pkt);
    }

    return 1; 
}

static int i = 0;

int Encoder::EncodeFrame(uint8_t** rgba_data, uint8_t **packets_data)
{
    int ret;

    ret = av_frame_make_writable(frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error frame not writtable\n");
        return ret;
    }

    int srcStride[4] = {width * 4 , 0, 0, 0}; 
    ret = sws_scale(sws_ctx, rgba_data,
        srcStride, 0, height, frame->data, frame->linesize
    );
    if (ret < 0)
    {
        fprintf(stderr, "Error while converting RGBA to YUV420P\n");
        return -1;
    }
    
    frame->pts = frame_index++;
    output_size = 0;
    ret = encode(enc_ctx, frame, pkt, &output_data, &output_size);
    if (ret < 0)
    {
        fprintf(stderr, "Error encoding frame\n");
        return -1;
    }

    if (output_size == 0)
    {
        *packets_data = nullptr;
        return 0;
    }

    *packets_data = output_data;
    return output_size;
}

Encoder::~Encoder()
{
    // Clean up
    avcodec_free_context(&enc_ctx);
    sws_freeContext(sws_ctx); // Free the conversion context
    av_frame_free(&frame);
    av_packet_free(&pkt);
    free(output_data);
}
