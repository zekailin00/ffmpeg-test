#include <unistd.h>
#include <arpa/inet.h>

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

int main(int argc, char **argv)
{
    const int WIDTH = 352, HEIGHT = 288;
    const int LINE_SIZE = WIDTH * 4;
    int sockfd = create_server();
    Encoder encoder{
        WIDTH, HEIGHT,400000,
        AV_CODEC_ID_MPEG2VIDEO,
        AV_PIX_FMT_YUV422P
    };

    uint8_t *rgba_data = (uint8_t *)malloc(WIDTH * HEIGHT * 4);

    int i, x, y;
    for (i = 0; i < 25 * 50; i++)
    {
        /* RGBA */
        for (y = 0; y < HEIGHT; y++) {
            for (x = 0; x < WIDTH; x++) {
                rgba_data[y * LINE_SIZE + x * 4 + 0] = x % 255;
                rgba_data[y * LINE_SIZE + x * 4 + 1] = y % 255;
                rgba_data[y * LINE_SIZE + x * 4 + 2] = i % 255;
                rgba_data[y * LINE_SIZE + x * 4 + 3] = 255;
            }
        }

        /* encode the image */
        uint8_t* packets_data = nullptr;
        int size = encoder.EncodeFrame(&rgba_data, &packets_data);

        if (size != 0 && packets_data != nullptr)
            send(clientSocket, packets_data, size, 0);
    }

    return 0;
}
