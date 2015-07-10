#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

// IP数据包首部
typedef struct {
    unsigned char  version_and_headlen;
    unsigned char  tos;
    unsigned short total_len;
    unsigned short id;
    unsigned short flag_and_offset;
    unsigned char  ttl;
    unsigned char  protocol;
    unsigned short checksum;
    unsigned int   src_ip;
    unsigned int   dst_ip;
}ip_header;

// 网际校验和算法
unsigned short checksum(unsigned short *buf,int nword) {
    unsigned long sum;
    for(sum=0;nword>0;nword--) sum += *buf++;
    sum = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);
    return ~sum;
}

// 这个函数用来测量到某个目标机器的网络的MTU
int calcu_mtu(char* target_addr) {
    int  ip_id         = 0;
    int  send_len      = 0;
    char buffer[65536] = {1};
    int  sockfd;
    struct sockaddr_in addr;

    sockfd = socket(PF_INET, SOCK_PACKET, SOCK_PACKET);
    if (sockfd < 0) {
        printf("创建原始套接字失败\r\n");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(target_addr);

    // 开始构造IP数据包
    ip_header *iph = (ip_header*)buffer;
    iph->version_and_headlen = (4<<4) + 5;
    iph->tos                 = 0;
    iph->total_len           = 20;
    iph->id                  = ip_id;
    iph->flag_and_offset     = 16384;
    iph->ttl                 = 128;
    iph->protocol            = IPPROTO_ICMP;
    iph->checksum            = 0;
    iph->src_ip              = inet_addr("192.168.137.192");
    iph->dst_ip              = inet_addr(target_addr);

    iph->checksum = checksum((unsigned short*)buffer, sizeof(ip_header));
    send_len = sendto(sockfd, buffer, sizeof(ip_header), 0, (struct sockaddr*)&addr, sizeof(struct sockaddr));
    if (send_len <= 0) {
        printf("发送IP数据包失败\r\n");
        close(sockfd);
        return -1;
    }
}

int main(int argc, char* argv[]) {
    calcu_mtu("204.79.197.200");
    return 0;
}
