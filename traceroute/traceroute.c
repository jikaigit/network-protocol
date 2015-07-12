#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

// IP报文首部
typedef struct {
    unsigned char  ver_and_hdl;
    unsigned char  tos;
    unsigned short total_len;
    unsigned short id;
    unsigned short flag_and_offset;
    unsigned char  ttl;
    unsigned char  protocol;
    unsigned short checksum;
    unsigned int   src_addr;
    unsigned int   dst_addr;
}ip_header;

// ICMP报文首部
typedef struct {
    unsigned char  type;
    unsigned char  code;
    unsigned short checksum;
    unsigned int   reserved;
}icmp_header;

typedef struct {
    ip_header   iph;
    icmp_header icmph;
}packet_header;

int parse_packet(packet_header* pkh, char* buffer, int length) {
    if (length < 28) {
        return -1;
    }
    ip_header iph;
    iph.ver_and_hdl     = buffer[0];
    iph.tos             = buffer[1];
    iph.total_len       = ((short)buffer[2]<<8) + buffer[3];
    iph.id              = ((short)buffer[4]<<8) + buffer[5];
    iph.flag_and_offset = ((short)buffer[6]<<8) + buffer[7];
    iph.ttl             = buffer[8];
    iph.protocol        = buffer[9];
    iph.checksum        = ((short)buffer[10]<<8) + buffer[11];
    iph.src_addr        = ((int)buffer[12]<<24) + ((int)buffer[13]<<16) + ((int)buffer[14]<<8) + buffer[15];
    iph.dst_addr        = ((int)buffer[16]<<24) + ((int)buffer[17]<<16) + ((int)buffer[18]<<8) + buffer[19];
    // debug_ip(iph);

    icmp_header icmph;
    icmph.type     = buffer[20];
    icmph.code     = buffer[21];
    icmph.checksum = ((short)buffer[22]<<8) + buffer[23];
    icmph.reserved = ((int)buffer[24]<<24) + ((int)buffer[25]<<16) + ((int)buffer[26]<<8) + buffer[27];
    // debug_icmp(icmph);

    packet_header pheader;
    pheader.iph   = iph;
    pheader.icmph = icmph;
    *pkh = pheader;
    return 1;
}

void debug_ip(ip_header iph) {
    printf("IP Header: %d\r\n", iph.ver_and_hdl >> 4);
    printf("Header Length: %d\r\n", iph.ver_and_hdl & 0xF);
    printf("Type Of Service: %d\r\n", iph.tos);
    printf("Total Length: %d\r\n", iph.total_len);
    printf("IP Identifier: %d\r\n", iph.id);
    printf("Flag And Offset:　%d\r\n", iph.flag_and_offset);
    printf("Time To Live: %d\r\n", iph.ttl);
    printf("Protocol: %d\r\n", iph.protocol);
    printf("Checksum: %d\r\n", iph.checksum);
    printf("Src Address: %u.%u.%u.%u\r\n",
        (iph.src_addr&0xF000)>>24,
        (iph.src_addr&0x0F00)>>16,
        (iph.src_addr&0x00F0)>>8,
        (iph.src_addr&0x000F)
    );
    printf("Dst Address: %u.%u.%u.%u\r\n",
        (iph.dst_addr&0xF000)>>24,
        (iph.dst_addr&0x0F00)>>16,
        (iph.dst_addr&0x00F0)>>8,
        (iph.dst_addr&0x000F)
    );
    printf("\r\n");
}

void debug_icmp(icmp_header icmph) {
    printf("ICMP Type: %d\r\n", icmph.type);
    printf("ICMP Code: %d\r\n", icmph.code);
    printf("Checksum: %d\r\n", icmph.checksum);
    printf("\r\n");
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
    struct sockaddr_in send_addr;
    struct sockaddr_in from_addr;

    // 设置接收套接字
    recv_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (recv_sockfd < 0) {
        printf("创建接收套接字失败\r\n");
        exit(EXIT_FAILURE);
    }
    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 100;
    //setsockopt(recv_sockfd, SOL_SOCKET, )

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

    int   ttl = 1;
    int   ttl_size = sizeof(ttl);
    int   send_len = 0;
    int   recv_len = 0;
    int   from_len = sizeof(from_addr);
    char  buffer[4096] = {0};
    unsigned char rmtaddr[4];
    for (;;) {
        // 设置数据包存活期
        setsockopt(send_sockfd, IPPROTO_IP, IP_TTL, (void*)&ttl, sizeof(ttl));

        send_len = sendto(send_sockfd, "hello", 5, 0, (struct sockaddr*)&send_addr, sizeof(struct sockaddr));
        if (send_len <= 0) {
            printf("发送数据失败\r\n");
            close(send_sockfd);
            close(recv_sockfd);
            exit(EXIT_FAILURE);
        }

        ttl++;
        if (ttl == 255) {
            break;
        }
    }

    packet_header pheader;
    for (;;) {
        recv_len = recvfrom(recv_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &from_len);
        if (recv_len > 0) {
            if (parse_packet(&pheader, buffer, recv_len) < 0) {
                continue;
            }
            if (pheader.iph.protocol != IPPROTO_ICMP) {
                continue;
            }
            if (pheader.icmph.type == 11) {
                printf("中途路由: %s\r\n", inet_ntoa(from_addr.sin_addr));
            } else if (pheader.icmph.type == 3 && pheader.icmph.code == 3) {
                printf("目标主机: %s\r\n", inet_ntoa(from_addr.sin_addr));
            }
        }
    }

    return 0;
}
