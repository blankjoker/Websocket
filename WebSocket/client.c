/*===============================================================
*   Copyright (C) 2018 All rights reserved.
*   
*   文件名称：server.c
*   创 建 者：zhangkunliang
*   创建日期：2018年12月25日
*   描    述：
*
*   更新日志：
*
================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])
{

	int sockfd, connfd;
	/*1. 创建套接字*/
	if (0 > (sockfd = socket(AF_INET, SOCK_STREAM, 0)))
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}
	printf("socoket........\n");

	/*2. 主动连接服务器*/
	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(6666);
	srv_addr.sin_addr.s_addr = inet_addr("172.26.184.244");
	if (0 > connect(sockfd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)))
	{
		perror("connect");
		exit(EXIT_FAILURE);
	}
	printf("connect.............\n");

	char buf[1024];
	int ret = 0;
	char buf_send[1024];
	while (1)
	{	
		//接收服务器的数据
		if (0 > (ret = recv(sockfd, buf, sizeof(buf), 0)))
		{
			perror("recv");
			break;
		}
		else if (0 == ret)
		{
			printf("write close!\n");
			break;
		}
		if(NULL == buf)
		{
			printf("write close!\n");
			break;
		}
		printf("POI:%s\n", buf);
		
		//发送数据到服务器
		//fgets(buf_send, sizeof(buf_send), stdin);
		send(sockfd, buf, sizeof(buf), 0);
		printf("send_server_data :%s\n", buf);
		printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		fflush(stdout);
		
	}
	close(sockfd);
	exit(0);
	return 0;
}
