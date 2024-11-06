#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>

#define PORT 12345
#define BUFFER_SIZE 4096

void save_frame_as_image(AVFrame *frame, int width, int height, int frame_index) {
    char filename[32];
    snprintf(filename, sizeof(filename), "frame_%04d.png", frame_index);

    AVCodec *png_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    AVCodecContext *png_context = avcodec_alloc_context3(png_codec);
    png_context->bit_rate = 400000;
    png_context->width = width;
    png_context->height = height;
    png_context->pix_fmt = AV_PIX_FMT_RGB24;
    png_context->time_base = (AVRational){1, 25};

    if (avcodec_open2(png_context, png_codec, NULL) < 0) {
        fprintf(stderr, "Could not open PNG codec.\n");
        return;
    }

    AVPacket *pkt = av_packet_alloc();
    avcodec_send_frame(png_context, frame);
    if (avcodec_receive_packet(png_context, pkt) == 0) {
        FILE *f = fopen(filename, "wb");
        fwrite(pkt->data, 1, pkt->size, f);
        fclose(f);
    }

    av_packet_free(&pkt);
    avcodec_free_context(&png_context);
}

int main() {
    avformat_network_init();

    // Set up a UDP socket for receiving video data
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        return -1;
    }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return -1;
    }

    // Initialize FFmpeg structures
    AVFormatContext *format_ctx = avformat_alloc_context();
    uint8_t buffer[BUFFER_SIZE];
    int frame_count = 0;
    struct SwsContext *sws_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int bytes_received;

    // Probe the input buffer to identify the format
    while (1) {
        bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) continue;

        // Create a ByteIOContext from the received data
        AVIOContext *avio_ctx = avio_alloc_context(buffer, bytes_received, 0, NULL, NULL, NULL, NULL);
        format_ctx->pb = avio_ctx;

        // Probe the format based on received data
        if (avformat_open_input(&format_ctx, NULL, NULL, NULL) < 0) {
            fprintf(stderr, "Could not probe input format\n");
            avformat_close_input(&format_ctx);
            avio_context_free(&avio_ctx);
            continue;
        }

        // Identify and initialize the codec context based on format
        AVCodec *codec = NULL;
        for (int i = 0; i < format_ctx->nb_streams; i++) {
            AVStream *stream = format_ctx->streams[i];
            codec = avcodec_find_decoder(stream->codecpar->codec_id);
            if (!codec) {
                fprintf(stderr, "Unsupported codec!\n");
                continue;
            }

            codec_ctx = avcodec_alloc_context3(codec);
            if (avcodec_parameters_to_context(codec_ctx, stream->codecpar) < 0) {
                fprintf(stderr, "Could not copy codec parameters\n");
                avcodec_free_context(&codec_ctx);
                continue;
            }

            if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
                fprintf(stderr, "Could not open codec\n");
                avcodec_free_context(&codec_ctx);
                continue;
            }

            // Initialize software scaler for RGB conversion
            sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                                     codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,
                                     SWS_BILINEAR, NULL, NULL, NULL);
            break; // Exit the loop if codec initialized successfully
        }

        // Clean up after probing if successful
        avformat_close_input(&format_ctx);
        avio_context_free(&avio_ctx);
        if (codec_ctx && sws_ctx) break; // Exit loop if initialization is complete
    }

    // Continue receiving and processing packets
    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        av_packet_from_data(packet, buffer, bytes_received);

        // Decode packet
        if (avcodec_send_packet(codec_ctx, packet) == 0) {
            while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                AVFrame *rgb_frame = av_frame_alloc();
                int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
                uint8_t *rgb_buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));
                av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buffer, AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);

                sws_scale(sws_ctx, (const uint8_t * const *)frame->data, frame->linesize, 0, codec_ctx->height, rgb_frame->data, rgb_frame->linesize);

                save_frame_as_image(rgb_frame, codec_ctx->width, codec_ctx->height, frame_count++);
                av_free(rgb_buffer);
                av_frame_free(&rgb_frame);
            }
        }
        av_packet_unref(packet);
    }

    // Clean up resources
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    av_frame_free(&frame);
    av_packet_free(&packet);
    close(sockfd);
    avformat_network_deinit();

    return 0;
}