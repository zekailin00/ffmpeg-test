

/**
 * receive from socket
 * parse codec 
 * decode packets [done]
 * render to imgui [done]
 */

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "inspector.h"

#include <unistd.h>
#include <arpa/inet.h>
#define PORT 12345
#define IP_ADDR "127.0.0.1"
int clientFd;



AVPacket *packet;
AVFrame *frame;
AVFrame *frame_rgb;

int video_stream_index = -1;
AVFormatContext *format_ctx = NULL;
AVCodecContext *codec_ctx;
struct SwsContext *sws_ctx;


void receiveLoop(void* callbackData, unsigned char** image, int* out_width, int* out_height);

int initializeCodec(AVFormatContext *fmt_ctx)
{
    video_stream_index = -1;
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        fprintf(stderr, "Could not find video stream.\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    AVCodecParameters *codec_params = fmt_ctx->streams[video_stream_index]->codecpar;
    const AVCodec *codec = (const AVCodec*)avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        fprintf(stderr, "Unsupported codec.\n");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codec_params);
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec.\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    return 0;
}


int readSocket(void *opaque, uint8_t *buf, int buf_size)
{
    int client_fd = *static_cast<int*>(opaque);
    int c = buf_size;
    unsigned char* cbuf = (unsigned char*)buf;
    do {
        int bytesRecv = recv(client_fd, cbuf, buf_size, 0);
        if (bytesRecv == 0)
            return AVERROR_EOF;
        cbuf += bytesRecv;
        buf_size -=bytesRecv;

    } while(buf_size > 0);
    return c;
}

int readFile(void *opaque, uint8_t *buf, int buf_size)
{
    FILE* f = (FILE*) opaque;
    ssize_t readSize = fread(buf, 1, buf_size, f);
    printf("file read size: %u; actual read size: %zu\n", buf_size, readSize);

    if (!readSize)
        return AVERROR_EOF;
    
    return FFMIN(buf_size, readSize);
}

int connectServer()
{
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
 
    // Convert IPv4 and IPv6 addresses from text to binary form
    inet_pton(AF_INET, IP_ADDR, &address.sin_addr);
    if (connect(client_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
        throw;
    
    return client_fd;
}


int main(int argc, char *argv[])
{
    const char *input_filename = "/home/zekailin00/Desktop/decoder/HW3.mp4";
    const char *output_filename_template = "frame_%04d.png";

    size_t avio_ctx_buffer_size = 4096;
    uint8_t *avio_ctx_buffer = (uint8_t*) av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        return AVERROR(ENOMEM);
    }

    // FILE* f = fopen(input_filename, "rb");
    // AVIOContext *avio_ctx = avio_alloc_context(
    //     avio_ctx_buffer, avio_ctx_buffer_size, 0,
    //     f, &readFile, NULL, NULL
    // );

    int fd = connectServer();
    AVIOContext *avio_ctx = avio_alloc_context(
        avio_ctx_buffer, avio_ctx_buffer_size, 0,
        &fd, &readSocket, NULL, NULL
    );

    if (!avio_ctx) {
        return AVERROR(ENOMEM);
    }

    if (!(format_ctx = avformat_alloc_context())) {
        return AVERROR(ENOMEM);
    }
    format_ctx->pb = avio_ctx;


    if (avformat_open_input(&format_ctx, NULL, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open input file.\n");
        return -1;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream info.\n");
        avformat_close_input(&format_ctx);
        return -1;
    }

    if (initializeCodec(format_ctx) < 0)
        return -1;
    
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    frame_rgb = av_frame_alloc();
    frame_rgb->format = AV_PIX_FMT_RGBA;
    frame_rgb->width  = codec_ctx->width;
    frame_rgb->height = codec_ctx->height;
    int ret = av_frame_get_buffer(frame_rgb, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }

    renderLoop(receiveLoop, nullptr);

    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    av_frame_free(&frame_rgb);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    if (avio_ctx)
        av_freep(&avio_ctx->buffer);
    avio_context_free(&avio_ctx);
    // fclose(f);


    return 0;
}


void receiveLoop(void* callbackData, unsigned char** image, int* out_width, int* out_height)
{

    static int frame_count = 0;
    char output_filename[1024];
    int ret;
    if (av_read_frame(format_ctx, packet) >= 0)
    {
        if (packet->stream_index == video_stream_index &&
            avcodec_send_packet(codec_ctx, packet) == 0)
        {
            while (avcodec_receive_frame(codec_ctx, frame) == 0)
            {
                ret = sws_scale(sws_ctx,
                    (uint8_t const * const *)frame->data, frame->linesize,
                    0, codec_ctx->height,
                    frame_rgb->data, frame_rgb->linesize
                );

                if (ret < 0)
                {
                    char error_buf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                    fprintf(stderr, "Error receiving packet: %s\n", error_buf);
                }

                frame_rgb->width = frame->width;
                frame_rgb->height = frame->height;
                frame_rgb->format = AV_PIX_FMT_RGBA;

                if (frame_rgb->data != nullptr)
                {
                    *image = frame_rgb->data[0];
                    *out_height = frame->height;
                    *out_width = frame->width;
                }

                frame_count++;
            }
        
        }
        av_packet_unref(packet);
    }
}