/* server.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "base64.h"
#include "sha1.h"
#include "intLib.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>



#define REQUEST_LEN_MAX 1024
#define DEFEULT_SERVER_PORT 8000
#define WEB_SOCKET_KEY_LEN_MAX 256
#define RESPONSE_HEADER_LEN_MAX 1024
#define LINE_MAX 256

char buffer[REQUEST_LEN_MAX] = {0};

void shakeHand(int connfd,const char *serverKey);
char * fetchSecKey(const char * buf);
char * computeAcceptKey(const char * buf);
char * analyData(const char * buf,const int bufLen);
char * packData(const char * message,unsigned long * len);
void response(const int connfd,const char * message);

int main(int argc, char *argv[])
{
	//pid_t pid = fork();

	int fd[2];
	pipe(fd);

	int memid = shmget(888, 100, 0666|IPC_CREAT);
	//ftok("/opt", 'x')

	int semid = semget(888, 1, 0666|IPC_CREAT);
	semctl(semid, 0, SETVAL, 0);

	if(-1 == memid)
	{
		perror("shmget");
		return -1;
	}

	if(fork())
	{
		printf("这是一个进程>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		struct sockaddr_in servaddr, cliaddr;
		socklen_t cliaddr_len;
		int listenfd, connfd;
		char buf[REQUEST_LEN_MAX];
		char *data = NULL;//char *data ;
		char str[INET_ADDRSTRLEN];
		char *secWebSocketKey;
		int i, n;
		int connected = 0;//0:not connect.1:connected.
		int port = DEFEULT_SERVER_PORT;

		if(argc > 1)
		{
			port = atoi(argv[1]);
		}
		if(port <= 0 || port > 0xFFFF)
		{
			printf("Port(%d) is out of range(1-%d)\n", port, 0xFFFF);
			return 0;
		}
		listenfd = socket(AF_INET, SOCK_STREAM, 0);

		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(port);

		bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

		listen(listenfd, 20);

		printf("Listen %d\nAccepting connections ...\n", port);
		cliaddr_len = sizeof(cliaddr);
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddr_len);
		//
		


		//测试用
		// printf("From %s at PORT %d\n",
		// inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
		// ntohs(cliaddr.sin_port));
		while (1)
		{



			memset(buf, 0, REQUEST_LEN_MAX);
			n = read(connfd, buf, REQUEST_LEN_MAX);	
			printf("----------------------------------------------------\n");

			//printf("%s\n", buf);
			if(0 == connected)
			{
				printf("read:%d\n%s\n", n, buf);
				secWebSocketKey = computeAcceptKey(buf);	
				shakeHand(connfd, secWebSocketKey);
				connected = 1;
				continue;
			}

			//打印客户端发送过来的信息
			data = analyData(buf, n);

			close(fd[0]);
			write(fd[1], buffer, sizeof(buffer));
			memset(buffer, 0, REQUEST_LEN_MAX);



			//信号量操作P-1
			struct sembuf op = {0};
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			semop(semid, &op, 1);

			//共享内存建立
			char *buff = NULL;
			buff = shmat(memid, NULL, 0);
			printf("buff:%s\n", buff);

			//服务器返回信息给网页
			response(connfd, buff);
			if(NULL == data)
			{
				printf("connect is close>>>>>>>>>>>>>>>>>>>>\n");
				break;
			}
			
		}
		printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		close(connfd);
		exit(0);
	}
	else
	{
		printf("这是一个进程>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		int sockfd, connfd1;

		/*1. 创建套接字*/
		if (0 > (sockfd = socket(AF_INET, SOCK_STREAM, 0)))
		{
			perror("socket");
			exit(EXIT_FAILURE);
		}
		printf("socoket........\n");

		int on  = 1;
		if (0 > setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
		{
			/* code */
			perror("setsockopt");
			return -1;
		}



		/*2. 绑定本机地址和端口号*/
		struct sockaddr_in myaddr;
		memset(&myaddr, 0, sizeof(myaddr));
		myaddr.sin_family = AF_INET;
		myaddr.sin_port = htons(6666);
		myaddr.sin_addr.s_addr = inet_addr("0.0.0.0");
		if (0 > bind(sockfd, (struct sockaddr*)&myaddr, sizeof(myaddr)))
		{
			perror("bind");
			exit(EXIT_FAILURE);
		}
		printf("bind.............\n");

		/*3. 设置监听套接字*/
		if (0 > listen(sockfd, 1024))
		{
			perror("listen");
			exit(EXIT_FAILURE);
		}
		printf("listen..........\n");

		/*4. 接收客户端连接，并生成通信套接字*/
		struct sockaddr_in cliaddr;
		int addrlen = sizeof(cliaddr);
		if (0 > (connfd1 = accept(sockfd, (struct sockaddr*)&cliaddr, &addrlen)))
		{
			perror("accept");
			exit(EXIT_FAILURE);
		}
		printf("accept: %s\n", inet_ntoa(cliaddr.sin_addr));

		char web_data[REQUEST_LEN_MAX] = {0};
		char buf_recv[REQUEST_LEN_MAX];
		int ret1;

		char *data = NULL;
		while (1)
		{


			//发送数据到客户端
			close(fd[1]);
			ret1 = read(fd[0], web_data, sizeof(web_data));

			if(0 == ret1)
			{
				printf("HTML5 is close>>>>>>>>>>>>>>>>\n");
				break;
			}	
			if (0 > send(connfd1, web_data, sizeof(web_data), 0))
			{
				perror("send");
				break;
			}


			//接收客户端反馈的数据
			ret1 = recv(connfd1, buf_recv, sizeof(buf_recv), 0);
			if(ret1){
				printf("recv_client_data :%s\n", buf_recv);
				fflush(stdout);
			//	data = buf_recv;
			//	printf("data:::%s\n", data);
			//	response(*connfd2, data);
			//	memset(data, 0, REQUEST_LEN_MAX);
			}


			//使用共享内存
			char *buff1 = NULL;
			buff1 = shmat(memid, NULL, 0);
			strcpy(buff1, buf_recv);

			//信号量操作V+1
			struct sembuf op = {0};
			op.sem_num = 0;
			op.sem_op = 1;
			op.sem_flg = 0;
			semop(semid, &op, 1);

		}

		printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		close(sockfd);
		close(connfd1);
		printf("please enter the computer.........\n");
		exit(0);

	}
	exit(0);
	return 0;
}



char * fetchSecKey(const char * buf)
{
	char *key;
	char *keyBegin;
	char *flag = "Sec-WebSocket-Key: ";
	int i = 0, bufLen = 0;

	key = (char *)malloc(WEB_SOCKET_KEY_LEN_MAX);
	memset(key, 0, WEB_SOCKET_KEY_LEN_MAX);
	if(!buf)
	{
		return NULL;
	}

	keyBegin = strstr(buf, flag);
	if(!keyBegin)
	{
		return NULL;
	}
	keyBegin += strlen(flag);

	bufLen = strlen(buf);
	for(i=0; i<bufLen; i++)
	{
		if(keyBegin[i] == 0x0A || keyBegin[i] == 0x0D)
		{
			break;
		}
		key[i] = keyBegin[i];
	}

	return key;
}

char * computeAcceptKey(const char * buf)
{
	char * clientKey;
	char * serverKey; 
	char * sha1DataTemp;
	char * sha1Data;
	short temp;
	int i,n;
	const char * GUID="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";


	if(!buf)
	{
		return NULL;
	}
	clientKey = (char *)malloc(LINE_MAX);
	memset(clientKey, 0, LINE_MAX);
	clientKey = fetchSecKey(buf);

	if(!clientKey)
	{
		return NULL;
	}


	strcat(clientKey, GUID);

	sha1DataTemp = sha1_hash(clientKey);
	n = strlen(sha1DataTemp);


	sha1Data = (char *)malloc(n/2+1);
	memset(sha1Data, 0, n/2+1);

	for(i=0; i<n; i+=2)
	{      
		sha1Data[i/2] = htoi(sha1DataTemp, i, 2);    
	} 

	serverKey = base64_encode(sha1Data, strlen(sha1Data)); 

	return serverKey;
}

void shakeHand(int connfd,const char *serverKey)
{
	char responseHeader [RESPONSE_HEADER_LEN_MAX];

	if(!connfd)
	{
		return;
	}

	if(!serverKey)
	{
		return;
	}

	memset(responseHeader,'\0',RESPONSE_HEADER_LEN_MAX);

	sprintf(responseHeader, "HTTP/1.1 101 Switching Protocols\r\n");
	sprintf(responseHeader, "%sUpgrade: websocket\r\n", responseHeader);
	sprintf(responseHeader, "%sConnection: Upgrade\r\n", responseHeader);
	sprintf(responseHeader, "%sSec-WebSocket-Accept: %s\r\n\r\n", responseHeader, serverKey);

	printf("Response Header:%s\n", responseHeader);

	write(connfd, responseHeader, strlen(responseHeader));
}




char * analyData(const char * buf,const int bufLen)
{
	char * data;
	char fin, maskFlag,masks[4];
	char * payloadData = NULL; //payloadData;
	char temp[8];
	unsigned long n, payloadLen = 0;
	unsigned short usLen = 0;
	int i = 0; 


	if (bufLen < 2) 
	{
		return NULL;
	}

	fin = (buf[0] & 0x80) == 0x80; // 1bit，1表示最后一帧  
	if (!fin)
	{
		return NULL;// 超过一帧暂不处理 
	}

	maskFlag = (buf[1] & 0x80) == 0x80; // 是否包含掩码  
	if (!maskFlag)
	{
		return NULL;// 不包含掩码的暂不处理
	}

	payloadLen = buf[1] & 0x7F; // 数据长度 
	if (payloadLen == 126)
	{      
		memcpy(masks, buf+4, 4);      
		payloadLen = (buf[2]&0xFF) << 8 | (buf[3]&0xFF);  
		payloadData = (char *)malloc(payloadLen);
		memset(payloadData, 0, payloadLen);
		memcpy(payloadData, buf+8, payloadLen);
	}
	else if (payloadLen == 127)
	{
		memcpy(masks, buf+10 ,4);  
		for ( i = 0; i < 8; i++)
		{
			temp[i] = buf[9 - i];
		} 

		memcpy(&n, temp, 8);  
		payloadData = (char *)malloc(n); 
		memset(payloadData, 0, n); 
		memcpy(payloadData, buf+14, n);//toggle error(core dumped) if data is too long.
		payloadLen = n;    
	}
	else
	{   
		memcpy(masks, buf+2, 4);    
		payloadData = (char *)malloc(payloadLen);
		memset(payloadData, 0, payloadLen);
		memcpy(payloadData, buf+6, payloadLen); 
	}

	for (i = 0; i < payloadLen; i++)
	{
		payloadData[i] = (char)(payloadData[i] ^ masks[i % 4]);
	}

	printf("data(%ld):%s\n",payloadLen, payloadData);
	strcpy(buffer, payloadData);

	//printf("buffer:%s\n", buffer);
	return payloadData;
}

//打包数据准备发送到网页
char *  packData(const char * message,unsigned long * len)
{
	char * data = NULL;
	unsigned long n;

	n = strlen(message);
	if (n < 126)
	{
		data = (char *)malloc(n+2);
		memset(data, 0, n+2);	 
		data[0] = 0x81;
		data[1] = n;
		memcpy(data+2, message, n);
		*len = n + 2;
	}
	else if (n < 0xFFFF)
	{
		data = (char *)malloc(n+4);
		memset(data, 0, n+4);
		data[0] = 0x81;
		data[1] = 126;
		data[2] = (n>>8 & 0xFF);
		data[3] = (n & 0xFF);
		memcpy(data+4, message, n);    
		*len = n + 4;
	}
	else
	{

		// 暂不处理超长内容  
		*len = 0;
	}


	return data;
}

void response(int connfd, const char * message)
{
	char *data = NULL;
	unsigned long n = 0;
	int i;
	if(!connfd)
	{
		return;
	}
	
	//出错的地方
	if(!message)
	{
		return;
	}
	
	//printf("text2>>>>>>>>>>\n");
	//data = (char *)malloc(REQUEST_LEN_MAX);
	//memset(data, 0, REQUEST_LEN_MAX);
	//memcpy(data, message, REQUEST_LEN_MAX);
	data = packData(message, &n); 
	//判断发送的数据是否为空
	if(!data || n <= 0)
	{
		printf("data is empty!\n");
		return;
	} 

	write(connfd, data, n);
}
