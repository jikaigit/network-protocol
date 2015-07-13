#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FILER_SERVER_PORT 7777

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage: file_client [filename]\r\n");
        exit(EXIT_SUCCESS);
    }

    char buff[4096] = {0};
    int client_sockfd;
    struct sockaddr_in connect_addr;

    client_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client_sockfd < 0) {
        printf("创建客户端socket失败\r\n");
        exit(EXIT_FAILURE);
    }

    connect_addr.sin_family = AF_INET;
    connect_addr.sin_port = htons(FILER_SERVER_PORT);
    connect_addr.sin_addr.s_addr = inet_addr("192.168.137.211");

    if (connect(client_sockfd, (struct sockaddr*)&connect_addr, sizeof(struct sockaddr)) < 0) {
        printf("连接失败\r\n");
        close(client_sockfd);
        exit(EXIT_FAILURE);
    }

    char* filename = argv[1];
    if (filename == "") {
        printf("文件名不能为空\r\n");
        close(client_sockfd);
        exit(EXIT_FAILURE);
    }
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printf("无法找到文件，文件可能不存在或文件损坏\r\n");
        close(client_sockfd);
        exit(EXIT_FAILURE);
    }

    // 先发送文件的文件名，好让服务器创建这个文件
    int send_len = strlen(filename);
    if (send_len >= 255) {
        printf("文件路径不能超过255哦\r\n");
        close(client_sockfd);
        exit(EXIT_FAILURE);
    }
    send_len = sprintf(buff, "%s/", filename);
    buff[send_len] = '\0';
    if (send(client_sockfd, buff, send_len, 0) <= 0) {
        printf("发送文件名失败\r\n");
        close(client_sockfd);
        exit(EXIT_FAILURE);
    }

    // 开始发送文件的数据
    char ch = 0;
    int  i  = 0;
    int  read_len = 0;
    for (;;) {
        ch = fgetc(file);

        if (ch == EOF) {
            buff[i] = '\0';
            send(client_sockfd, buff, strlen(buff), 0);
            printf("传输已完毕\r\n");
            break;
        }

        buff[i] = ch;
        i++;

        if (i == 4095) {
            buff[i] = '\0';
            i = 0;
            send(client_sockfd, buff, strlen(buff), 0);
        }
    }

    close(client_sockfd);
    fclose(file);
    return 0;
}
