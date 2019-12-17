/**
* clent.c
* 客户端程序的功能比较简单，实现了一个简单的回射
* 客户端向服务器发送一个字母'A'，服务器再传回字母'A'  
*/ 
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
 int sockfd;
 int len;
 struct sockaddr_in address;
 int result;
 char ch = 'A';
 int port_no;

 printf("Please input port: \n");
 scanf("%d", &port_no);

 sockfd = socket(AF_INET, SOCK_STREAM, 0);
 address.sin_family = AF_INET;
 address.sin_addr.s_addr = inet_addr("127.0.0.1");
 address.sin_port = htons(port_no);
 len = sizeof(address);
 result = connect(sockfd, (struct sockaddr *)&address, len);

 if (result == -1)
 {
  perror("oops: client1");
  exit(1);
 }
 printf("Connection established!\n");
 write(sockfd, &ch, 1);
 printf("Send successfully!\n");
 read(sockfd, &ch, 1);
 printf("char from server = %c\n", ch);
 close(sockfd);
 exit(0);
}

