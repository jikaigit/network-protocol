#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

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
    unsigned char  type;
    unsigned char  code;
    unsigned short checksum;
    unsigned short id;
    unsigned short seq;
}icmp_echo_header;

// IP首部+ICMP首部的组合
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
    char send_buff[65535] = {0};
    char recv_buff[65535] = {0};
    int sockfd;
    struct sockaddr_in send_addr;
    struct sockaddr_in recv_addr;

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        printf("创建原始套接字失败\r\n");
        exit(EXIT_FAILURE);
    }

    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 10;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    bzero(&send_addr, sizeof(send_addr));
    bzero(&recv_addr, sizeof(send_addr));

    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = inet_addr(argv[1]);

    int set = IP_PMTUDISC_DO;
    setsockopt(sockfd, IPPROTO_IP, IP_MTU_DISCOVER, &set, sizeof(set));

    int send_len = 0;
    int recv_len = 0;
    int seq      = 1;
    int mtu_low  = 0;
    int mtu_high = 1466;
    int mtu_val  = 1466;
    int pid      = getpid();
    int sin_size = sizeof(struct sockaddr);
    packet_header pheader;
    for (;;) {
        icmp_echo_header icmp_echoh;
        icmp_echoh.type     = 8;
        icmp_echoh.code     = 0;
        icmp_echoh.checksum = 0;
        icmp_echoh.id       = pid;
        icmp_echoh.seq      = seq;

        memcpy(send_buff, &icmp_echoh, sizeof(icmp_echo_header));
        icmp_echoh.checksum = checksum((short*)send_buff, sizeof(icmp_echo_header));
        memcpy(send_buff, &icmp_echoh, sizeof(icmp_echo_header));

        // debug
        printf("本次发送长度: %d\r\n", mtu_val+34);

        send_len = sendto(sockfd, send_buff, mtu_val, 0, (struct sockaddr*)&send_addr, sizeof(send_addr));
        if (send_len < 0) {
            printf("发送数据失败\r\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        for (;;) {
            recv_len = recvfrom(sockfd, recv_buff, sizeof(recv_buff), 0, (struct sockaddr*)&recv_addr, &sin_size);
            if (recv_len < 0) {
                printf("接收数据失败\r\n");
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            packet_header pkh;
            if (parse_packet(&pkh, recv_buff, recv_len) < 0) {
                printf("数据包解析失败\r\n");
                close(sockfd);
                continue;
            }
            if (pkh.iph.protocol != IPPROTO_ICMP) {
                continue;
            }
            if (pkh.icmph.type == 0 && send_addr.sin_addr.s_addr == recv_addr.sin_addr.s_addr) {
                mtu_low  = (mtu_low+mtu_high)/2+1;
                mtu_val  =  mtu_low;
                if (mtu_low >= mtu_high) {
                    printf("到目的网络的MTU为: %d\r\n", mtu_low + 34);
                    close(sockfd);
                    exit(EXIT_SUCCESS);
                }
                break;
            } else if (pkh.icmph.type == 3 && pkh.icmph.code == 4) {
                mtu_high = (mtu_low+mtu_high)/2;
                mtu_val  =  mtu_high;
                break;
            }
        }

        seq++;
        sleep(1);
    }

    return 0;
}
