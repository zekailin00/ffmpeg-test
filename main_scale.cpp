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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main()
{
    std::string filename = "/home/zekailin00/Desktop/decoder/scratch/build/pic.jpg";
    int imgX, imgY, imgN;
    unsigned char *data = stbi_load(filename.c_str(), &imgX, &imgY, &imgN, 0);

    int WIDTH = imgX, HEIGHT = imgY;
    int IMG_CHN = imgN;
    int LINE_SIZE = WIDTH * IMG_CHN;

    AVPixelFormat dst_fmt = AV_PIX_FMT_RGB24;
    AVPixelFormat src_fmt =  AV_PIX_FMT_YUV420P; //AV_PIX_FMT_YUV444P;
    int ret;

    SwsContext * ctx_rgba_yuv = sws_getContext(
        WIDTH, HEIGHT, dst_fmt,
        WIDTH, HEIGHT, src_fmt,
        0, 0, 0, 0
    );

    SwsContext * ctx_yuv_rgba = sws_getContext(
        WIDTH, HEIGHT, src_fmt,
        WIDTH, HEIGHT, dst_fmt,
        0, 0, 0, 0
    );

    AVFrame* frame_yuv = av_frame_alloc();
    if (!frame_yuv) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(5);
    }
    frame_yuv->format = src_fmt;
    frame_yuv->width  = WIDTH;
    frame_yuv->height = HEIGHT;
    ret = av_frame_get_buffer(frame_yuv, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        throw "Could not allocate the video frame data";
    }  

    AVFrame* frame_rgba = av_frame_alloc();
    if (!frame_rgba) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(5);
    }
    frame_rgba->format = dst_fmt;
    frame_rgba->width  = WIDTH;
    frame_rgba->height = HEIGHT;
    ret = av_frame_get_buffer(frame_rgba, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        throw "Could not allocate the video frame data";
    }

    uint8_t* rgba_data[1];
    rgba_data[0] = (uint8_t*) malloc(WIDTH * HEIGHT * IMG_CHN);


    for (int i = 0; i < 25 * 10; i++)
    {
        // /* RGBA */
        // for (int y = 0; y < HEIGHT; y++)
        // {
        //     for (int x = 0; x < WIDTH; x++)
        //     {
        //         frame_rgba->data[0][y * LINE_SIZE + x * 4 + 0] = (x * i) % 255;
        //         frame_rgba->data[0][y * LINE_SIZE + x * 4 + 1] = (y * i) % 255;
        //         frame_rgba->data[0][y * LINE_SIZE + x * 4 + 2] = 0;
        //         frame_rgba->data[0][y * LINE_SIZE + x * 4 + 3] = 255;
        //     }
        // }

        uint8_t *pos = frame_rgba->data[0];
        // for (int y = 0; y < HEIGHT; y++)
        // {
        //     for (int x = 0; x < WIDTH; x++)
        //     {
        //         pos[0] = i / (float)25 * 255;
        //         pos[1] = 0;
        //         pos[2] = x / (float)(WIDTH) * 255;
        //         pos[3] = 255;
        //         pos += 4;
        //     }
        // }

        memcpy(pos, data, WIDTH * HEIGHT * IMG_CHN);

        // /* prepare a dummy image */
        // /* Y */
        // for (int y = 0; y < HEIGHT; y++) {
        //     for (int x = 0; x < WIDTH; x++) {
        //         frame_yuv->data[0][y * frame_yuv->linesize[0] + x] = x + y + i * 3;
        //     }
        // }
        // //
        // //        /* Cb and Cr */
        // for (int y = 0; y < HEIGHT/2; y++) {
        //     for (int x = 0; x < WIDTH/2; x++) {
        //         frame_yuv->data[1][y * frame_yuv->linesize[1] + x] = 128 + y + i * 2;
        //         frame_yuv->data[2][y * frame_yuv->linesize[2] + x] = 64 + x + i * 5;
        //     }
        // }

        filename = "img_raw_" + std::to_string(i) + ".jpg";
        stbi_write_jpg(filename.c_str(), WIDTH, HEIGHT, IMG_CHN, frame_rgba->data[0], 100);

        int inLinesize[1] = { LINE_SIZE }; // RGBA stride
        sws_scale(
            ctx_rgba_yuv,
            frame_rgba->data, frame_rgba->linesize, //rgba_data, inLinesize,
            0, HEIGHT,
            frame_yuv->data, frame_yuv->linesize
        );

        // sws_scale(
        //     ctx_yuv_rgba,
        //     frame_yuv->data, frame_yuv->linesize,
        //     0, HEIGHT,
        //     frame_rgba->data, frame_rgba->linesize
        // );

        // filename = "img_raw_" + std::to_string(i) + ".jpg";
        // stbi_write_jpg(filename.c_str(), WIDTH, HEIGHT, IMG_CHN, frame_rgba->data[0], 100);

        // int inLinesize[1] = { LINE_SIZE }; // RGBA stride
        // sws_scale(
        //     ctx_rgba_yuv,
        //     frame_rgba->data, frame_rgba->linesize, //rgba_data, inLinesize,
        //     0, HEIGHT,
        //     frame_yuv->data, frame_yuv->linesize
        // );

        sws_scale(
            ctx_yuv_rgba,
            frame_yuv->data, frame_yuv->linesize,
            0, HEIGHT,
            frame_rgba->data, frame_rgba->linesize
        );

        filename = "img_scaled_" + std::to_string(i) + ".jpg";
        stbi_write_jpg(filename.c_str(), WIDTH, HEIGHT, IMG_CHN, frame_rgba->data[0], 100);

    }

}
