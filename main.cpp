

#include "decoder.h"
#include "inspector.h"

#include <unistd.h>
#include <arpa/inet.h>
#define PORT 12345
#define IP_ADDR "127.0.0.1"
int clientFd;


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
    FILE* f = *static_cast<FILE**>(opaque);
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

void receiveLoop(void* callbackData, unsigned char** image, int* out_width, int* out_height)
{
    Decoder* decoder = (Decoder*) callbackData;
    decoder->GetLatestFrame(image, out_width, out_height);
}

int main(int argc, char *argv[])
{
    const char *input_filename = "/home/zekailin00/Desktop/decoder/HW3.mp4";

    // int fd = connectServer();
    // Decoder decoder(&fd, readSocket);

    FILE* fd = fopen(input_filename, "rb");
    Decoder decoder(&fd, readFile);

    renderLoop(receiveLoop, &decoder);

    return 0;
}
