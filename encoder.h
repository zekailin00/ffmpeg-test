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
    Encoder(int width, int height, int crf, int framerate, AVCodecID codec_id, AVPixelFormat PIX_FMT);
    ~Encoder();
    
    /**
     * @brief encode RGBA frames to a compressed packet stream
     * 
     * @param rgba_data Input RGBA frame buffer
     * @param packets_data Output buffer containing packets data
     * @return int the size of output buffer
     */
    int EncodeFrame(uint8_t** rgba_data, uint8_t **packets_data);

    void SetFixation(
        float xFixLeft, float yFixLeft,
        float xFixRight, float yFixRight,
        bool isStereo = true)
    {
        this->isStereo  = isStereo;
        this->xFixLeft  = xFixLeft;
        this->yFixLeft  = yFixLeft;
        this->xFixRight = xFixRight;
        this->yFixRight = yFixRight;
    }

    void SetFoveationProp(
        float qpOffsetStrength = 30,
        float stdGaussianWidth = 0.2)
    {
        this->qpOffsetStrength = qpOffsetStrength;
        this->stdGaussianWidth = stdGaussianWidth;
    }

private:
    AVCodecContext *enc_ctx;
    const AVCodec *codec;
    SwsContext *sws_ctx;
    AVFrame *frame;
    AVPacket *pkt;

    uint8_t *output_data;
    int output_size;

    bool isStereo = true;
    float xFixLeft = 0.5f, yFixLeft = 0.5f;
    float xFixRight = 0.5f, yFixRight = 0.5f;
    float qpOffsetStrength = 30;
    float stdGaussianWidth = 0.2;

    int width, height;
    int frame_index;

    const AVPixelFormat PIX_FMT;

    static int encode(
        AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, 
        uint8_t **output, int *output_size
    );

    static void set_foveation(
        AVFrame *frame, bool isStereo,
        float delta, float sigma,
        float xFixLeft, float yFixLeft,
        float xFixRight, float yFixRight);
};
