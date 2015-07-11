#include <stdio.h>		//perror()
#include <stdlib.h>
#include <netdb.h>		//getprotobynumber  gethostbyname
#include <unistd.h>		//setuid() getuid() sleep() alarm()
#include <sys/types.h>	//getuid()
#include <string.h>		//bzero(s, n) 将s的前n个字节设为0
#include <signal.h>		//signal()
#include <time.h>		//gettimeofday()
#include <netinet/ip_icmp.h>
#include <errno.h>		//EINTR
#include <arpa/inet.h>	//inet_ntoa()

#define MAX_NO_PACKETS	10
#define PACKET_SIZE		4096
#define MAX_WAIT_TIME	5

int sockfd;
int datalen = 56;
struct sockaddr_in dest_addr;
struct sockaddr_in from;
pid_t pid;//获取进程号

int nsend = 0, nreceived = 0;

char sendpacket[PACKET_SIZE];
char recvpacket[PACKET_SIZE];

struct timeval tvrecv;

void statistics();				//显示发送数据包的总结信息
void send_packet(int count);	//发送ICMP报文
void recv_packet(int count);	//接收所有ICMP报文

int main(int argc, char** argv)
{
	struct protoent* protocol;
	int size = 1024*50;
	unsigned int inaddr = 0l;//0L
	struct hostent *host;

	if (argc < 3)
	{
		printf("usage:%s hostname/IP address or usage:%s hostname/IP address -r\n", argv[0], argv[0]);
		exit(1);
	}

	//getprotobyname()会返回一个protoent结构，参数proto为欲查询的网络协议名。
	//此函数会从 /etc/protocols中查找符合条件的数据并由结构protoent返回
	if ((protocol=getprotobyname("icmp")) == NULL)
	{
		perror("getprotobyname");
		exit(1);
	}

	//生成使用ICMP的原始套接字,这种套接字只有root用户才能生成
	if ((sockfd=socket(AF_INET, SOCK_RAW, protocol->p_proto)) < 0)
	{
		perror("socket error");
		exit(1);
	}

	//setuid(getuid()); //回收root权限，设置当前用户权限

	//扩大套接字接收缓冲区到50K这样做主要为了减小接收缓冲区溢出的的可能性,若无意中ping一个广播地址或多播地址,将会引来大量应答
	//SO_RCVBUF int 为接收确定缓冲区大小
	//选项定义的层次,目前仅支持SOL_SOCKET和IPPROTO_TCP层次。
	int val = IP_PMTUDISC_DO;
    setsockopt(sockfd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

	bzero(&dest_addr, sizeof(dest_addr));

	//关于sockaddr_in为成员
	//sa_family是地址家族，一般都是“AF_xxx”的形式。通常大多用的是都是AF_INET,代表TCP/IP协议族。
	//sin_port存储端口号（使用网络字节顺序）
	//sin_addr存储IP地址，使用in_addr这个数据结构
	dest_addr.sin_family = AF_INET;

	inaddr = inet_addr(argv[2]);//将一个点分十进制的IP转换成一个长整数型数

	//判断是主机名还是ip地址
	if (inaddr == INADDR_NONE)
	{
		if ((host=gethostbyname(argv[2])) == NULL)
		{
			perror("gethostbyname error");
			exit(1);
		}
		//是主机地址
		memcpy((char*)&dest_addr.sin_addr, host->h_addr, sizeof(dest_addr.sin_addr));//没有h_addr这个成员，但是h_addr表示h_addr_list的第一个地址,因为#define h_addr h_addr_list[0]
	}
	else//是ip地址
	{
		memcpy((char *)&dest_addr.sin_addr, (char *)&inaddr, sizeof(inaddr));
	}

	//获取main的进程id，用于设置icmp的标志符
	pid = getpid();

	printf("PING %s(%s): %d bytes data in ICMP packets.\n", argv[2], inet_ntoa(dest_addr.sin_addr), datalen);

	//设置某一信号的对应动作
	//SIGINT:由Interrupt Key产生，通常是CTRL+C或者DELETE。发送给所有ForeGround Group的进程
	signal(SIGINT, statistics);

	if (argc == 3)
	{
		send_packet(5);
		recv_packet(5);
		statistics();
	}
	else
	{
		printf("input error, pls check it...\n");
	}
	return 0;
}

/***********************************************************************/
//FUNCTION:校验和算法
unsigned short cal_chksum(unsigned short *addr, int len)
{
	int nleft = len;
	int sum = 0;
	unsigned short *w = addr;
	unsigned short answer = 0;

	//把ICMP报头二进制数据以2字节为单位累加起来
	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	//若ICMP报头为奇数个字节，会剩下最后一字节。把最后一个字节视为一个2字节数据的高字节，
	//这个2字节数据的低字节为0，继续累加
	if (nleft == 1)
	{
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}

	sum = (sum>>16) + (sum&0xffff);
	sum += (sum>>16);
	answer = ~sum;

	return answer;
}

/***********************************************************************/
//FUNCTION:两个timeval结构相减
void tv_sub(struct timeval *out,struct timeval *in)
{
	if ((out->tv_usec-=in->tv_usec) < 0)
	{
		--out->tv_sec;
		out->tv_usec += 1000000;
	}

	out->tv_sec -= in->tv_sec;
}

/***********************************************************************/
//FUNCTION:设置ICMP报头
int pack(int pack_no)
{
	int i, packsize;
	struct icmp *icmp;
	struct timeval *tval;

	icmp = (struct icmp *)sendpacket;
	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_cksum = 0;
	icmp->icmp_seq = pack_no;
	icmp->icmp_id = pid;

	packsize = datalen + 8;
	tval = (struct timeval *)icmp->icmp_data;
	gettimeofday(tval, NULL);	//记录发送时间

	icmp->icmp_cksum = cal_chksum((unsigned short *)icmp, packsize);

	return packsize;
}

/***********************************************************************/
//FUNCTION:剥去ICMP报头加显示
int unpack(char *buf, int len)
{
	int i, iphdrlen;
	struct ip *ip;
	struct icmp *icmp;
	struct timeval *tvsend;
	double rtt;

	ip = (struct ip *)buf;
	iphdrlen = ip->ip_hl<<2;//求ip报头长度,即ip报头的长度标志乘4
	icmp = (struct icmp *)(buf+iphdrlen);//越过ip报头,指向ICMP报头

	len -= iphdrlen;	//ICMP报头及ICMP数据报的总长度
	if (len < 8)		//小于ICMP报头长度则不合理
	{
		printf("ICMP packets\'s length is less than 8\n");
		return -1;
	}

	//确保所接收的是我所发的的ICMP的回应
	if ((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id == pid))
	{
		printf("这里显示MTU\r\n");
	}
	else if ((icmp->icmp_type == 3) && (icmp->icmp_id == pid)) {
        printf("这里应该增大MTY\r\n");
	}
	else
		return -1;
}

/***********************************************************************/
//FUNCTION:
void statistics()
{
	printf("\n------------------------ping statistics------------------------\n");
	printf("%d packets transmitted, %d received, %.2f%% packets lost\n", nsend, nreceived, (float)(nsend-nreceived)/(float)nsend*100);
	printf("---------------------------------------------------------------\n");
	close(sockfd);
	exit(1);
}

/***********************************************************************/
//FUNCTION:
void send_packet(int count)
{
	int packetsize;

	while (nsend < MAX_NO_PACKETS)//发送MAX_NO_PACKETS个报文
	{
		nsend++;
		packetsize = pack(nsend);

		//sendpacket为要发送的内容，由pack()函数设定，dest_addr是目的地址
		if (sendto(sockfd, sendpacket, packetsize, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
		{
			perror("sendto error");
			continue;
		}
		//sleep(1);//每隔一秒发送一个ICMP报文
	}
}

/***********************************************************************/
//FUNCTION:接收所有ICMP报文
void recv_packet(int count)
{
	int n, fromlen;
	extern int errno;

	//用alarm函数设置的timer超时或setitimer函数设置的interval timer超时
	signal(SIGALRM, statistics);

	fromlen = sizeof(from);
	while (nreceived < nsend)
	{
		//alarm()用来设置信号SIGALRM在经过参数seconds指定的秒数后传送给目前的进程
		alarm(MAX_WAIT_TIME);

		//(经socket接收数据
		if ((n=recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&from, &fromlen)) < 0)
		{
			if (errno == EINTR)
				continue;
			perror("recvfrom error");
			continue;
		}

		gettimeofday(&tvrecv, NULL);//记录接收时间

		if (unpack(recvpacket, n) == -1)
			continue;

		nreceived++;
	}
}
