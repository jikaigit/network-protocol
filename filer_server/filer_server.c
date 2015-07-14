#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FILER_SERVER_PORT 7777

typedef struct {
    int client_sockfd;
    struct sockaddr_in client_addr;
}thread_param;

void* file_recv(void* arg) {
    thread_param* param = (thread_param*)arg;
    char  buff[4096]    = {0};
    char  remain[4096]  = {0};
    char  filename[255] = {0};
    int   filename_i = 0;
    int   recv_len   = 0;
    int   i, j       = 0;
    FILE* file;
    int   first = 1;

    // 开始接收文件的数据
    for (;;) {
        recv_len = recv(param->client_sockfd, buff, sizeof(buff)-1, 0);
        if (recv_len < 0) {
            printf("接收错误\r\n");
            close(param->client_sockfd);
            fclose(file);
            free(param);
            return (void*)-1;
        }
        if (recv_len == 0) {
            break;
        }
        // 第一次接收要解析文件名
        if (first == 1) {
            for (i = 0; ; i++) {
                if (buff[i] == '/') {
                    first = 0;
                    i++;
                    break;
                }
                if (i >= recv_len) {
                    continue;
                }
                filename[filename_i] = buff[i];
                filename_i++;
            }
            file = fopen(filename, "wb");
            if (file == NULL) {
                printf("创建文件失败\r\n");
                close(param->client_sockfd);
                free(param);
                return (void*)-1;
            }
            if (i < recv_len) {
                for (; i < recv_len; i++) {
                    remain[j] = buff[i];
                    j++;
                }
                remain[j] = '\0';
                fprintf(file, "%s", remain);
            }
        }
        else {
            buff[recv_len] = '\0';
            fprintf(file, "%s", buff);
        }
    }
    printf("已经完成接收客户%s的文件\r\n", inet_ntoa(param->client_addr.sin_addr));

    fclose(file);
    close(param->client_sockfd);
    free(param);
    return (void*)0;
}

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
    for (;;) {
        accept_sockfd = accept(server_sockfd, (struct sockaddr*)&accept_addr, &sin_size);
        if (accept_sockfd < 0) {
            printf("接收客户端%s的连接失败\r\n", inet_ntoa(accept_addr.sin_addr));
            continue;
        }
        pthread_t pid;
        thread_param* param = (thread_param*)malloc(sizeof(thread_param));
        param->client_sockfd = accept_sockfd;
        param->client_addr   = accept_addr;
        pthread_create(&pid, NULL, file_recv, (void*)param);
    }

    close(accept_sockfd);
    close(server_sockfd);
    return 0;
}
