#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define NONE           -1
#define OVER_TIME      1
#define CAN_NOT_ARRIVE 2

// ICMP不可达首部定义
typedef struct {
    unsigned char  type;
    unsigned char  code;
    unsigned short checksum;
    unsigned int   reserved;
}icmp_header;

int parse_icmp(char* buff, unsigned char rmtaddr[4]) {
    int iph_len   = buff[0] & 0xF;
    int iph_bytes = iph_len*4;
    int icmp_type = buff[iph_bytes];
    rmtaddr[0] = buff[12];
    rmtaddr[1] = buff[13];
    rmtaddr[2] = buff[14];
    rmtaddr[3] = buff[15];
    if (icmp_type == 3 && buff[iph_bytes+1] == 3)  return CAN_NOT_ARRIVE;
    if (icmp_type == 11) return OVER_TIME;
    return NONE;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage: %s [ip|domain]\r\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    int ttl     = 1;
    int ttl_len = sizeof(int);
    int recv_sockfd;
    struct sockaddr_in lisaddr;
    struct sockaddr_in addr;

    recv_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (recv_sockfd < 0) {
        printf("创建接受套接字失败\r\n");
        exit(EXIT_FAILURE);
    }
    lisaddr.sin_family = AF_INET;
    lisaddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(recv_sockfd, (struct sockaddr*)&lisaddr, sizeof(struct sockaddr)) < 0) {
        printf("绑定失败\r\n");
        close(recv_sockfd);
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port   = htons(64234);
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        struct hostent *ht = gethostbyname(argv[1]);
        addr.sin_addr.s_addr = inet_addr(ht->h_addr_list[0]);
    }

    int  send_len     = 0;
    int  recv_len     = 0;
    char buffer[4096] = {0};
    int  sin_size     = sizeof(struct sockaddr);
    for (;;) {
        int send_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (send_sockfd < 0) {
            printf("创建发送套接字失败\r\n");
            close(recv_sockfd);
            exit(EXIT_FAILURE);
        }

        setsockopt(send_sockfd, IPPROTO_IP, IP_TTL, (void*)&ttl, sizeof(ttl));

        send_len = sendto(send_sockfd, "hello", 5, 0, (struct sockaddr*)&addr, sizeof(struct sockaddr));
        if (send_len <= 0) {
            printf("发送数据失败\r\n");
            close(send_sockfd);
            close(recv_sockfd);
            exit(EXIT_FAILURE);
        }

        // 根据返回的数据包判定是否应该输出当前主机
        recv_len = recvfrom(recv_sockfd, buffer, 4096, 0, (struct sockaddr*)&addr, &sin_size);
        if (recv_len <= 0) {
            printf("接受数据失败\r\n");
            close(send_sockfd);
            close(recv_sockfd);
            exit(EXIT_FAILURE);
        }
        buffer[recv_len] = '\0';
        unsigned char rmtaddr[4] = {0};
        switch (parse_icmp(buffer, rmtaddr)) {
        case OVER_TIME:
            printf("中途路由:%u.%u.%u.%u\r\n", rmtaddr[0], rmtaddr[1], rmtaddr[2], rmtaddr[3]);
            break;

        case CAN_NOT_ARRIVE:
            printf("目标主机:%u.%u.%u.%u\r\n", rmtaddr[0], rmtaddr[1], rmtaddr[2], rmtaddr[3]);
            close(send_sockfd);
            close(recv_sockfd);
            return 0;

        case NONE:
            printf("不是预期的ICMP类型\r\n");
            close(send_sockfd);
            close(recv_sockfd);
            exit(EXIT_FAILURE);
        }

        close(send_sockfd);
        ttl++;
    }

    return 0;
}
