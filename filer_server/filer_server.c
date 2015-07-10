#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FILER_SERVER_PORT 7777

int main() {
    int server_sockfd;
    int accept_sockfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in accept_addr;

    server_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server_sockfd < 0) {
        printf("创建服务端socket失败");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(FILER_SERVER_PORT);
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);

    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0) {
        printf("将服务端socket绑定服务器地址失败");
        close(server_sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sockfd, 10) < 0) {
        printf("服务器监听失败");
        close(server_sockfd);
        exit(EXIT_FAILURE);
    }

    int sin_size = sizeof(accept_addr);
    accept_sockfd = accept(server_sockfd, (struct sockaddr*)&accept_addr, &sin_size);
    if (accept_sockfd <= 0) {
        printf("接收连接失败\r\n");
        close(server_sockfd);
        exit(EXIT_FAILURE);
    }

    char buff[4096];
    int  recv_len = 0;

    //　先接收一下文件名
    recv_len = recv(accept_sockfd, buff, sizeof(buff), 0);
    buff[recv_len] = '\0';
    if (recv_len <= 0) {
        printf("接收文件名失败\r\n");
        close(server_sockfd);
        exit(EXIT_FAILURE);
    }

    // 然后创建文件(相同的文件名)
    FILE* file = fopen(buff, "w");
    if (file == NULL) {
        printf("创建文件失败\r\n");
        close(server_sockfd);
        exit(EXIT_FAILURE);
    }
    printf("创建文件成功\r\n");

    // 开始接收文件的数据
    for (;;) {
        memset(buff, '\0', 4096);
        recv_len = recv(accept_sockfd, buff, sizeof(buff), 0);
        if (recv_len <= 0) {
            printf("已经完成接收\r\n");
            break;
        }
        fprintf(file, "%s", buff);
    }

    close(accept_sockfd);
    close(server_sockfd);
    fclose(file);
    return 0;
}
