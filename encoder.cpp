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
#include <string>
#include <algorithm>

Encoder::Encoder(
    int width, int height, int crf, int framerate,
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
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "aq-mode", "1", 0);
    // av_dict_set(&opts, "profile", "main", 0);
    av_dict_set(&opts, "tune", "zerolatency", 0);
    av_dict_set(&opts, "preset", "ultrafast", 0);
    crf = std::max(0, std::min(51, crf));
    av_dict_set(&opts, "crf", std::to_string(crf).c_str(), 0);

    // enc_ctx->bit_rate = bit_rate; // automatically switch to X264_RC_ABR when set
    // enc_ctx->rc_max_rate = bit_rate;
    enc_ctx->width = width;
    enc_ctx->height = height;
    // enc_ctx->gop_size = 10;
    // enc_ctx->max_b_frames = 1;
    enc_ctx->pix_fmt = Encoder::PIX_FMT;

    // FIXME: variable framerate
    enc_ctx->time_base = (AVRational){1, framerate};
    enc_ctx->framerate = (AVRational){framerate, 1};

    // Open the codec
    if (avcodec_open2(enc_ctx, codec, &opts) < 0)
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

#include <libavutil/frame.h>
#include <libavutil/mem.h>

void set_regions_of_interest(AVFrame *frame, int width, int height) {
    // Number of regions
    int num_rois = 1;
    
    // Allocate memory for regions
    AVRegionOfInterest *rois = (AVRegionOfInterest*)av_calloc(num_rois, sizeof(AVRegionOfInterest));
    if (!rois) {
        fprintf(stderr, "Failed to allocate memory for ROIs\n");
        return;
    }
    
    // Define a single ROI
    rois[0].top = height / 4;
    rois[0].bottom = height / 4 * 3;
    rois[0].left = width / 4;
    rois[0].right = width / 4 * 3;
    rois[0].self_size = sizeof(AVRegionOfInterest);
    rois[0].qoffset = {2, 3}; // Higher quality for this region

    // Add the ROI data to the frame
    AVFrameSideData *side_data = av_frame_new_side_data(
        frame, AV_FRAME_DATA_REGIONS_OF_INTEREST,
        num_rois * sizeof(AVRegionOfInterest)
    );
    if (!side_data) {
        fprintf(stderr, "Failed to allocate side data for ROIs\n");
        av_free(rois);
        return;
    }
    memcpy(side_data->data, rois, num_rois * sizeof(AVRegionOfInterest));

    // Free the temporary ROI data
    av_free(rois);
}

void Encoder::set_foveation(
    AVFrame *frame, bool isStereo,
    float delta, float sigma,
    float xFixLeft, float yFixLeft,
    float xFixRight, float yFixRight)
{
    AVFoveationInfo *fov = (AVFoveationInfo*)av_calloc(1, sizeof(AVFoveationInfo));
    if (!fov) {
        fprintf(stderr, "Failed to allocate memory for foveation\n");
        return;
    }
    
    // Define a foveation
    fov->isStereo  = isStereo;
    fov->xFixLeft  = xFixLeft;
    fov->yFixLeft  = yFixLeft;
    fov->xFixRight = xFixRight;
    fov->yFixRight = yFixRight;
    fov->delta = delta;
    fov->sigma = sigma;

    // Add the ROI data to the frame
    AVFrameSideData *side_data = av_frame_new_side_data(
        frame, AV_FRAME_DATA_REGIONS_OF_INTEREST, sizeof(AVFoveationInfo)
    );
    if (!side_data) {
        fprintf(stderr, "Failed to allocate side data for foveation\n");
        av_free(fov);
        return;
    }
    memcpy(side_data->data, fov, sizeof(AVFoveationInfo));

    // Free the temporary foveation data
    av_free(fov);
}

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

    // set_regions_of_interest(frame, width, height);
    Encoder::set_foveation(
        frame, isStereo,
        qpOffsetStrength, stdGaussianWidth,
        xFixLeft, yFixLeft,
        xFixRight, yFixRight
    );
    
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


//     if (size == sizeof(AVFoveationInfo))
//     {
//         /* Foveated Video Encoding */
//         const AVFoveationInfo* foveation = (const AVFoveationInfo*)data;
//         qoffsets = av_calloc(mbx * mby, sizeof(*qoffsets));
//         if (!qoffsets)
//             return AVERROR(ENOMEM);

//         float x_fix, y_fix;
//         float sigma = foveation->sigma, delta = foveation->delta;

//         if (foveation->isStereo)
//         {
//             int mbxHalf = mbx * 0.5;
//             sigma =  sigma * sqrt(pow(mbxHalf, 2) + pow(mby, 2));

//             x_fix = foveation->xFixLeft * mbxHalf;
//             y_fix = foveation->yFixLeft * mby;
//             for (int x = 0; x < mbxHalf; x++) {
//                 for (int y = 0; y < mby; y++) {
//                     float gaussian = exp( - (pow(x - x_fix, 2) + pow(y - y_fix, 2)) / pow(sigma, 2));
//                     qoffsets[x + y * mbx] = av_clipf(delta * (1 - gaussian), 0, +qp_range);
//                 }
//             }

//             x_fix = foveation->xFixRight * mbxHalf + mbxHalf;
//             y_fix = foveation->yFixRight * mby;
//             for (int x = mbxHalf; x < mbx; x++) {
//                 for (int y = 0; y < mby; y++) {
//                     float gaussian = exp( - (pow(x - x_fix, 2) + pow(y - y_fix, 2)) / pow(sigma, 2));
//                     qoffsets[x + y * mbx] = av_clipf(delta * (1 - gaussian), 0, +qp_range);
//                 }
//             }

//             goto FVE_END;
//         }

//         x_fix = foveation->xFixLeft * mbx;
//         y_fix = foveation->yFixLeft * mby;
//         sigma =  sigma * sqrt(pow(mbx, 2) + pow(mby, 2));

//         for (int x = 0; x < mbx; x++) {
//             for (int y = 0; y < mby; y++) {
//                 float gaussian = exp( - (pow(x - x_fix, 2) + pow(y - y_fix, 2)) / pow(sigma, 2));
//                 qoffsets[x + y * mbx] = av_clipf(delta * (1 - gaussian), 0, +qp_range);
//             }
//         }
//         goto FVE_END;
//     }

// typedef struct AVFoveationInfo {
//     /**
//      * Must be set to the size of this data structure (that is,
//      * sizeof(AVRegionOfInterest)).
//      */
//     uint32_t self_size;

//     /**
//      * Stereo if both left and right foveations are used;
//      * otherwise, only left foveation is used.
//      */
//     int isStereo;

//     /**
//      * Relative fixation point, horizontal. 0 is left, 1 is right.
//      */
//     float xFixLeft, xFixRight;

//     /**
//      * Relative fixation point, vertical. 0 is top, 1 is bottom.
//      */
//     float yFixLeft, yFixRight;


//     /**
//      * Standard deviation of the gaussian. 1 equals the video diagonal.
//      */
//     float sigma;

//     /**
//      * Maximal quality difference between the (mostly) unaffected fixation center
//      * and the limit of the gaussian in QP
//      */
//     float delta;

// } AVFoveationInfo;
