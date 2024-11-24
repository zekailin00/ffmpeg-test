extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


class Encoder
{
public:
    Encoder(int width, int height, int64_t bit_rate, AVCodecID codec_id);
    ~Encoder();
    
    /**
     * @brief encode RGBA frames to a compressed packet stream
     * 
     * @param rgba_data Input RGBA frame buffer
     * @param packets_data Output buffer containing packets data
     * @return int the size of output buffer
     */
    int EncodeFrame(uint8_t** rgba_data, uint8_t **packets_data);

private:
    AVCodecContext *enc_ctx;
    const AVCodec *codec;
    SwsContext *sws_ctx;
    SwsContext * sws_ctx_yuv_rgba;
    AVFrame *frame;
    AVPacket *pkt;

    uint8_t *output_data;
    int output_size;

    int width, height;
    int frame_index;

    static int encode(
        AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, 
        uint8_t **output, int *output_size
    );
};
