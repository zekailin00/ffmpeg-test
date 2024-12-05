#include <unistd.h>
#include <arpa/inet.h>

#include "decoder.h"
#include "encoder.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define PORT 12345
int clientSocket;

int create_server()
{
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);
    socklen_t addrlen = sizeof(servaddr);

    // Set up a UDP socket for receiving video data
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        return -1;
    }
    int opt = 1;
    // Forcefully attaching socket to the port
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    if (bind(sockfd, (struct sockaddr*)&servaddr, addrlen) < 0) {
        perror("bind failed");
        close(sockfd);
        throw;
        // return -1;
    }
    listen(sockfd, 3);

    clientSocket = accept(sockfd, (struct sockaddr*)&servaddr, &addrlen);

    return sockfd;
}

void close_server(int sockfd)
{
    close(sockfd);
}

int readFile(void *opaque, uint8_t *buf, int buf_size)
{
    FILE* f = *static_cast<FILE**>(opaque);
    ssize_t readSize = fread(buf, 1, buf_size, f);
    printf("file read size: %u; actual read size: %zu\n", buf_size, readSize);

    if (!readSize)
        return AVERROR_EOF;
    
    return FFMIN(buf_size, readSize);
}

int main(int argc, char **argv)
{
    const char *input_filename = "/home/zekailin00/Desktop/decoder/Big_Buck_Bunny_1080_10s_30MB.mp4";
    FILE* fd = fopen(input_filename, "rb");
    Decoder decoder(&fd, readFile);

    unsigned char* image;
    int out_width;
    int out_height;
    while(!decoder.GetLatestFrame(&image, &out_width, &out_height));

    Encoder encoder{
        out_width, out_height, 100000,
        AV_CODEC_ID_H264,
        AV_PIX_FMT_YUV422P
    };
    int sockfd = create_server();

    int max_frames = 1000;
    while (max_frames--)
    {
        if (!decoder.GetLatestFrame(&image, &out_width, &out_height))
            continue;

        /* encode the image */
        uint8_t* packets_data = nullptr;
        int size = encoder.EncodeFrame(&image, &packets_data);

        if (size != 0 && packets_data != nullptr)
            send(clientSocket, packets_data, size, 0);
    }

    return 0;
}
