#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

// 用来解析数据包
typedef struct {
    // IP部分
    unsigned char  ver_and_headlen;
    unsigned char  tos;
    unsigned short total_len;
    unsigned short id;
    unsigned short flag_and_offset;
    unsigned char  ttl;
    unsigned char  protocol;
    unsigned short checksum;
    unsigned int   src_ip;
    unsigned int   dst_ip;

    // ICMP部分
    unsigned char  icmp_type;
    unsigned char  icmp_code;
    unsigned short icmp_checksum;
    unsigned short icmp_id;
    unsigned short icmp_seq;
}parse_header;

// ICMP数据包首部
typedef struct {
    unsigned char  type;
    unsigned char  code;
    unsigned short checksum;
    unsigned short id;
    unsigned short seq;
}icmp_header;

// 网际校验和算法
unsigned short checksum(unsigned short *buf,int nword) {
    unsigned long sum;
    for(sum = 0; nword > 0; nword--) sum += *buf++;
    sum = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);
    return ~sum;
}

// 这个函数用来测量到某个目标机器的网络的MTU
int calcu_mtu(char* target_addr) {
    char buffer[65535] = {0};
    int sockfd;
    struct sockaddr_in addr;
    struct sockaddr_in from;

    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        printf("不能创建原始套接字\r\n");
        return -1;
    }

    int val = IP_PMTUDISC_DO;
    setsockopt(sockfd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(target_addr);

    icmp_header *icmph = (icmp_header*)buffer;
    icmph->type     = 8;
    icmph->code     = 0;
    icmph->checksum = 0;
    icmph->id       = htons(0);
    icmph->seq      = htons(0);
    icmph->checksum = checksum((short*)buffer, sizeof(icmp_header));

    int sin_size = sizeof(struct sockaddr);
    int send_len;
    int recv_len;
    int mtu = 5000;
    struct sockaddr_in fromaddr;
    for (;;) {
        send_len = sendto(sockfd, buffer, sizeof(icmp_header), 0, (struct sockaddr*)&addr, sizeof(struct sockaddr));
        if (send_len <= 0) {
            printf("发送数据失败\r\n");
            close(sockfd);
            return -1;
        }

        for (;;) {
            recv_len = recvfrom(sockfd, buffer, 65534, 0, (struct sockaddr*)&from, &sin_size);
            if (recv_len > 0) {
                if (from.sin_addr.s_addr != addr.sin_addr.s_addr) {
                    printf("不是想要的数据包\r\n");
                    break;
                } else {
                    printf("是想要的包\r\n");
                    break;
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    calcu_mtu("192.168.137.11");
    return 0;
}
