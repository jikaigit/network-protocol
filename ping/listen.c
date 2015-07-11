#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
    int ls_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (ls_sockfd < 0) {
        printf("socket error.\r\n");
        return 0;
    }
    char buffer[4096];
    struct sockaddr_in from;
    int from_len = sizeof(from);
    for (;;) {
        int recv_len = recvfrom(ls_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&from, &from_len);
        if (recv_len > 0) {
            printf("data from: %s\r\n", inet_ntoa(from.sin_addr));
        }
    }
    return 0;
}
