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

int main(int argc, char *argv[])
{
    const char *input_filename = "/home/zekailin00/Desktop/decoder/HW3.mp4";
    const char *output_filename_template = "frame_%04d.png";

    // av_register_all();

    AVFormatContext *format_ctx = NULL;
    if (avformat_open_input(&format_ctx, input_filename, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open input file.\n");
        return -1;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream info.\n");
        avformat_close_input(&format_ctx);
        return -1;
    }

    int video_stream_index = -1;
    for (int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        fprintf(stderr, "Could not find video stream.\n");
        avformat_close_input(&format_ctx);
        return -1;
    }

    AVCodecParameters *codec_params = format_ctx->streams[video_stream_index]->codecpar;
    const AVCodec *codec = (const AVCodec*)avcodec_find_decoder(codec_params->codec_id);
    if (!codec) {
        fprintf(stderr, "Unsupported codec.\n");
        avformat_close_input(&format_ctx);
        return -1;
    }

    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codec_params);
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec.\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return -1;
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *frame_rgb = av_frame_alloc();

    int width = codec_ctx->width;
    int height = codec_ctx->height;
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
    uint8_t *buffer = (uint8_t *) av_malloc(num_bytes * sizeof(uint8_t));

    av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, buffer, AV_PIX_FMT_RGBA, width, height, 1);

    struct SwsContext *sws_ctx = sws_getContext(width, height, codec_ctx->pix_fmt, width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);

    int frame_count = 0;
    char output_filename[1024];
    int ret;

    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            if (avcodec_send_packet(codec_ctx, packet) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    ret = sws_scale(sws_ctx, (uint8_t const * const *)frame->data, frame->linesize, 0, height, frame_rgb->data, frame_rgb->linesize);
                    if (ret < 0)
                    {
                        char error_buf[AV_ERROR_MAX_STRING_SIZE];
                        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                        fprintf(stderr, "Error receiving packet: %s\n", error_buf);
                    }
                    frame_rgb->width = frame->width;
                    frame_rgb->height = frame->height;
                    frame_rgb->format = AV_PIX_FMT_RGBA;

                    snprintf(output_filename, sizeof(output_filename), output_filename_template, frame_count);
                    FILE *file = fopen(output_filename, "wb");
                    if (!file) {
                        fprintf(stderr, "Could not open output file.\n");
                        break;
                    }

                    const AVCodec *png_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
                    AVCodecContext *png_ctx = avcodec_alloc_context3(png_codec);
                    png_ctx->bit_rate = codec_ctx->bit_rate;
                    png_ctx->width = width;
                    png_ctx->height = height;
                    png_ctx->pix_fmt = AV_PIX_FMT_RGBA;
                    png_ctx->time_base = format_ctx->streams[video_stream_index]->time_base;

                    if (avcodec_open2(png_ctx, png_codec, NULL) < 0) {
                        fprintf(stderr, "Could not open PNG codec.\n");
                        fclose(file);
                        avcodec_free_context(&png_ctx);
                        break;
                    }

                    AVPacket *png_packet = av_packet_alloc();
                    avcodec_send_frame(png_ctx, frame_rgb);
                    if ((ret = avcodec_receive_packet(png_ctx, png_packet)) == 0) {
                        fwrite(png_packet->data, 1, png_packet->size, file);
                        av_packet_unref(png_packet);
                    }
                    else
                    {
                        char error_buf[AV_ERROR_MAX_STRING_SIZE];
                        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                        fprintf(stderr, "Error receiving packet: %s\n", error_buf);
                    }

                    fclose(file);
                    av_packet_free(&png_packet);
                    avcodec_free_context(&png_ctx);
                    frame_count++;
                }
            }
        }
        av_packet_unref(packet);
    }

    sws_freeContext(sws_ctx);
    av_free(buffer);
    av_frame_free(&frame);
    av_frame_free(&frame_rgb);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    return 0;
}