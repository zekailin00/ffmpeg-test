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


Encoder::Encoder(int width, int height, int64_t bit_rate, AVCodecID codec_id)
{
    this->width = width;
    this->height = height;
    frame_index = 0;

    // codec
    codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        return;
    }

    enc_ctx = avcodec_alloc_context3(codec);
    if (!enc_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return;
    }

    // Set encoding parameters 
    enc_ctx->bit_rate = bit_rate;
    enc_ctx->width = width;
    enc_ctx->height = height;
    enc_ctx->gop_size = 10;
    enc_ctx->max_b_frames = 1;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    // FIXME: variable framerate
    enc_ctx->time_base = (AVRational){1, 25};
    enc_ctx->framerate = (AVRational){25, 1};

    // Open the codec
    if (avcodec_open2(enc_ctx, codec, NULL) < 0)
    {
        fprintf(stderr, "Could not open codec\n");
        return;
    }

    // Allocate video frame
    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Could not allocate video frame\n");
        return;
    }

    frame->format = enc_ctx->pix_fmt;
    frame->width  = enc_ctx->width;
    frame->height = enc_ctx->height;

    sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_RGBA,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    sws_ctx_yuv_rgba = sws_getContext(
        width, height, AV_PIX_FMT_YUV420P,
        width, height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    if (!sws_ctx) {
        fprintf(stderr, "Could not initialize the conversion context\n");
        return;
    }

    int ret;
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }

    pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "Could not allocate packet\n");
        return;
    }

    output_size = 0;
    output_data = (uint8_t *)malloc(pkt->size);
    if (!output_data)
    {
        fprintf(stderr, "Failed to allocate memory for encoded data\n");
        return;
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

    int srcStride[4] = { width * 4 , 0, 0, 0}; 
    ret = sws_scale(sws_ctx, rgba_data,
        srcStride, 0, height, frame->data, frame->linesize
    );

    int srcStride2[4] = { width , width / 2, width / 2, 0};
    int tmp_linesize[4] = { width , width / 2, width / 2, 0};
    uint8_t* tmp_data[8];
    tmp_data[0] = (uint8_t*) malloc(width * height);
    tmp_data[1] = (uint8_t*) malloc(width * height);
    tmp_data[2] = (uint8_t*) malloc(width * height);
    sws_scale(sws_ctx_yuv_rgba, frame->data, frame->linesize,
        0, height, tmp_data, tmp_linesize
    );

    fflush(stdout);

    // /* Make sure the frame data is writable.
    //     On the first round, the frame is fresh from av_frame_get_buffer()
    //     and therefore we know it is writable.
    //     But on the next rounds, encode() will have called
    //     avcodec_send_frame(), and the codec may have kept a reference to
    //     the frame in its internal structures, that makes the frame
    //     unwritable.
    //     av_frame_make_writable() checks that and allocates a new buffer
    //     for the frame only if necessary.
    //     */
    // ret = av_frame_make_writable(frame);
    // if (ret < 0)
    //     exit(1);

    // /* Prepare a dummy image.
    //     In real code, this is where you would have your own logic for
    //     filling the frame. FFmpeg does not care what you put in the
    //     frame.
    //     */
    // /* Y */
    // for (int y = 0; y < height; y++) {
    //     for (int x = 0; x < width; x++) {
    //         frame->data[0][y * frame->linesize[0] + x] = x + y + (i % 25) * 3;
    //     }
    // }

    // /* Cb and Cr */
    // for (int y = 0; y < height/2; y++) {
    //     for (int x = 0; x < width/2; x++) {
    //         frame->data[1][y * frame->linesize[1] + x] = 128 + y + (i % 25) * 2;
    //         frame->data[2][y * frame->linesize[2] + x] = 64 + x + (i % 25) * 5;
    //     }
    // }

    // frame->pts = i++;

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

// int main() {
   
//     uint8_t* rgba_data = (uint8_t *)malloc(640 * 480 * 4);
//     memset(rgba_data, 0, 640 * 480 * 4);
//     const AVCodecID codec_id = AV_CODEC_ID_H264;
//     Encoder encoder(640, 480, 400000, codec_id);
//     uint8_t *output_data;
//     for(int i = 0; i < 45; i += 1) {
//         encoder.EncodeFrame(rgba_data, &output_data);
//     }

//     return 0;
// }
