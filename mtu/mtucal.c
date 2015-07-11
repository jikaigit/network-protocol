#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

// IP数据包首部
typedef struct {
    unsigned char  ver_and_ihl;
    unsigned char  tos;
    unsigned short total_len;
    unsigned short id;
    unsigned short flag_and_offset; // 16384
    unsigned char  ttl;
    unsigned char  protocol;
    unsigned short checksum;
    unsigned int   src_ip;
    unsigned int   dst_ip;
}ip_header;

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
    for(sum=0;nword>0;nword--) sum += *buf++;
    sum = (sum>>16) + (sum&0xffff);
    sum += (sum>>16);
    return ~sum;
}

// 这个函数用来测量到某个目标机器的网络的MTU
int calcu_mtu(char* target_addr) {
    char buffer[65536] = {0};
    int sockfd;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        printf("不能创建原始套接字\r\n");
        return -1;
    }

    int set = 1;
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, (void*)&set, sizeof(int));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(target_addr);

    ip_header iph;
    iph.ver_and_ihl     = (4<<4)+5;
    iph.tos             = 0;
    iph.total_len       = htons(28);
    iph.id              = htons(0);
    iph.flag_and_offset = htons(16384);
    iph.ttl             = 64;
    iph.protocol        = IPPROTO_ICMP;
    iph.checksum        = 0;
    iph.src_ip          = inet_addr("192.168.137.211");
    iph.dst_ip          = inet_addr(target_addr);
    memcpy(buffer, &iph, sizeof(ip_header));
    iph.checksum = htons(checksum((short*)buffer, sizeof(ip_header)));

    icmp_header icmph;
    icmph.type     = 8;
    icmph.code     = 0;
    icmph.checksum = 0;
    icmph.id       = 0;
    icmph.seq      = 0;
    memcpy(buffer, &iph, sizeof(icmp_header));
    icmph.checksum = checksum((short*)buffer, sizeof(icmp_header));

    memcpy(buffer, &iph, sizeof(ip_header));
    memcpy(buffer+sizeof(ip_header), &icmph, sizeof(icmp_header));

    // debug portion...
    int i = 0;
    printf("version: %d\r\n", buffer[0]>>4);
    printf("length of head: %d\r\n", buffer[0]&0xF);
    printf("type of service:　%d\r\n", buffer[1]);
    printf("total of length: %d\r\n", ((short)buffer[2]<<8)+buffer[3]);
    printf("identifier: %d\r\n", ((short)buffer[4]<<8)+buffer[5]);
    printf("flag and offset: %d\r\n", ((short)buffer[6]<<8)+buffer[7]);
    printf("time to live: %d\r\n", buffer[8]);
    printf("protocol: %d\r\n", buffer[9]);
    printf("check sum: %d\r\n", ((short)buffer[10]<<8)+buffer[11]);
    printf("source ip: %u.%u.%u.%u\r\n", );
    return -1;

    int send_len = sendto(sockfd, buffer, sizeof(ip_header), 0, (struct sockaddr*)&addr, sizeof(struct sockaddr));
    if (send_len <= 0) {
        printf("发送数据失败\r\n");
        close(sockfd);
    }
    printf("success\r\n");
}

int main(int argc, char* argv[]) {
    calcu_mtu("204.79.197.200");
    return 0;
}
