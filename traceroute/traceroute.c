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
    unsigned short id;
    unsigned short seq;
}icmp_echo_header;

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

// 网际校验和算法
unsigned short checksum(unsigned short *buf,int nword) {
    unsigned long sum;
    for(sum = 0; nword > 0; nword--) sum += *buf++;
    sum = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);
    return ~sum;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage: %s [ip|domain]\r\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    int send_sockfd;
    int recv_sockfd;
    struct sockaddr_in laddr;
    struct sockaddr_in send_addr;
    struct sockaddr_in from_addr;

    // 设置接收套接字
    recv_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (recv_sockfd < 0) {
        printf("创建接收套接字失败\r\n");
        exit(EXIT_FAILURE);
    }
    bzero(&laddr, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_addr.s_addr = inet_addr("192.168.137.211");
    /*if (bind(recv_sockfd, (struct sockaddr*)&laddr, sizeof(struct sockaddr)) < 0) {
        printf("为接收套接字绑定地址失败\r\n");
        close(recv_sockfd);
        exit(EXIT_FAILURE);
    }*/

    // 设置发送套接字
    send_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (send_sockfd < 0) {
        printf("创建发送套接字失败\r\n");
        close(recv_sockfd);
        exit(EXIT_FAILURE);
    }
    bzero(&send_addr, sizeof(send_addr));
    send_addr.sin_family = AF_INET;
    send_addr.sin_port   = htons(59801);
    send_addr.sin_addr.s_addr = inet_addr(argv[1]);

    int   seq = 1;
    int   ttl = 1;
    int   ttl_size = sizeof(ttl);
    int   send_len = 0;
    int   recv_len = 0;
    int   from_len = sizeof(from_addr);
    pid_t pid      = getpid();
    char  buffer[4096] = {0};
    unsigned char rmtaddr[4];
    for (;;) {
        // 设置数据包信息
        setsockopt(send_sockfd, IPPROTO_IP, IP_TTL, (void*)&ttl, sizeof(ttl));
        icmp_echo_header *icmpechoh = (icmp_echo_header*)buffer;
        icmpechoh->type     = 8;
        icmpechoh->code     = 0;
        icmpechoh->checksum = 0;
        icmpechoh->id       = pid;
        icmpechoh->seq      = seq;
        icmpechoh->checksum = checksum((short*)buffer, sizeof(icmp_echo_header));

        send_len = sendto(send_sockfd, "hello", 5, 0, (struct sockaddr*)&send_addr, sizeof(struct sockaddr));
        if (send_len <= 0) {
            printf("发送数据失败\r\n");
            close(send_sockfd);
            close(recv_sockfd);
            exit(EXIT_FAILURE);
        }

        // 根据返回的数据包判定是否应该输出当前主机
        for (;;) {
            recv_len = recvfrom(recv_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &from_len);
            if (recv_len > 0) {
                printf("接收到了数据\r\n");
                return 0;
            }
        }
        ttl++;
        seq++;
    }

    return 0;
}
