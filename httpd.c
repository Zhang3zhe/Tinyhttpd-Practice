/**
 * httpd.c
 * This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * Rewrite December 2019 by Zhangzhe
 * This program compiles for Ubuntu 16.04
 */
#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<ctype.h>
#include<strings.h>
#include<string.h>
#include<sys/stat.h>
#include<pthread.h>
#include<sys/wait.h>
#include<stdlib.h>

#define ISspace(x) isspace((int)(x))	// 若 x 为空格字符，返回 true 
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void* accept_request(void*);	        // 处理从套接字上监听到的 HTTP 请求 
void  bad_request(int);	                // 返回给客户端：这是个错误请求，HTTP 状态码 400 BAD REQUEST 
void  cat(int, FILE*);                  // 读取服务器上某个文件并写到套接字 
void  cannot_execute(int);              // 处理在执行 cgi 程序时出现的错误 
void  error_die(const char*);           // 把错误信息写到 perror 并退出 
void  execute_cgi(int, const char*, const char*, const char*);	  // 处理 cgi 脚本 
int   get_line(int, char*, int);        // 读取套接字的一行，把回车换行等情况统一为以换行符结束 
void  headers(int, const char*);        // 把 HTTP 响应报文的头部写到套接字 
void  not_found(int);                   // 处理找不到请求文件时的情况 
void  serve_file(int, const char*);     // 调用 cat，把服务器文件返回给浏览器 
int   startup(u_short*);               // 初始化 HTTP 服务，包括建立套接字、绑定端口、进行监听等 
void  unimplemented(int);               // 返回给浏览器：收到的 HTTP 请求所用的 method 不被支持 

int main(){
	int server_sock = -1;
	u_short port = 0;
	int client_sock = -1;
	struct sockaddr_in client_name;    // 套接字地址结构，定义在 <netinet/in.h>
	socklen_t client_name_len = sizeof(client_name);
	pthread_t newthread;
	server_sock = startup(&port);
	printf("Tinyhttpd running on port %d\n", port);
	// 多线程并发服务器模型
	while(1){
		// 阻塞等待客户端的连接 
		// 主线程 
		client_sock = accept(server_sock, (struct sockaddr*)&client_name, &client_name_len);
		if(client_sock == -1)
			error_die("accept");
		// 创建工作线程，执行回调函数 accept_request，参数为 client_sock  
		if(pthread_create(&newthread, NULL, accept_request, (void*)&client_sock) != 0)
			perror("pthread_create");
	}
	close(server_sock);	   // 关闭套接字，从协议栈角度看，即关闭 TCP 连接 
	return 0;
}

/**
 * This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket
 */
int startup(u_short* port){
	int httpd = 0;
	struct sockaddr_in name;
	// socket() 用于创建一个 socket 描述符，函数包含于 <sys/socket.h> 中 
	// PF_INET 与 AF_INET 同义 
	httpd = socket(PF_INET, SOCK_STREAM, 0);
	if(httpd == -1)
		error_die("socket");
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;	  // 地址族
	// 指定端口
	// 将 *port 转换成以网络字节序表示的16位整数 
	name.sin_port = htons(*port);
	// INADDR_ANY 是一个 IPv4 通配地址常量
	// 大多数实现将其定义成 0.0.0.0  
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	// bind() 用于绑定地址和 socket
	// 如果传进去的 sin_port 为0，这时系统会选择一个临时的端口号 
	if(bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0)
		error_die("bind");
	// 如果调用 bind() 后端口号仍然为0，则手动调用 getsockname() 获取端口号 
	if(*port == 0){
		socklen_t namelen = sizeof(name);
		if(getsockname(httpd, (struct sockaddr*)&name, &namelen) == -1)
			error_die("getsockname");
		*port = ntohs(name.sin_port); // 网络字节序转换为主机字节序 
	}
	// 服务器监听客户端请求，套接字排队的最大连接个数为5 
	if(listen(httpd, 5) < 0) 
		error_die("listen");
	return httpd;
}

/**
 * Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error.
 */
void error_die(const char* sc){
	perror(sc);
	exit(1);
}

/**
 * A request has caused a call to accept() on the server port to return.
 * Process the request appropriately.
 * Parameters: the socket connected to the client
 */
void* accept_request(void* tclient){
	int client = *(int*)tclient;
	char buf[1024];
	int numchars;
	char method[255];	// 请求方法 GET or POST
	char url[255];	    // 请求的文件路径
	char path[512];     // 文件相对路径
	size_t i, j;
	struct stat st;
	int cgi = 0;		//如果是 CGI 程序，则 cgi 为 true
	char* query_string = NULL;
	numchars = get_line(client, buf, sizeof(buf));  //从 client 中读取指定大小数据到 buf
	i = 0, j = 0;
	// 解析 http 请求报文
	// 接收字符处理：提取空格前的字符，至多254个
	while(!ISspace(buf[j]) && (i < sizeof(method)-1))
		method[i++] = buf[j++];
	method[i] = '\0';
	// 忽略大小写比较字符串，如果不是 GET 或 POST 中任何一个， 
	// 则直接发送 response 告诉客户端没实现该方法
	if(strcasecmp(method, "GET") && strcasecmp(method, "POST")){
		unimplemented(client);
		return NULL;
	}
	// 如果是 POST 方法，就把 cgi 标志位置为 true
	if(strcasecmp(method, "POST") == 0)
		cgi = 1;
	i = 0;
	// 过滤掉空格字符，空格后面是 url
	while(ISspace(buf[j]) && (j < sizeof(buf)))
		++ j;
	// 将部分中的非空格字符转存进 url 缓冲区，遇空格字符或满退出
	while(!ISspace(buf[j]) && (i < sizeof(url)-1) && (j < sizeof(buf)))
		url[i++] = buf[j++];
	url[i] = '\0';
	if(strcasecmp(method, "GET") == 0){	   // GET 类型 
		query_string = url;
		// 截取 '?' 前的字符，如果遍历完成也没找到字符 '?'，则退出循环
		while((*query_string != '?') && (*query_string != '\0'))
			++ query_string;
		if(*query_string == '?'){
			cgi = 1;	             // 如果是 '?'，说明这个请求需要调用 cgi
			*query_string = '\0'; 	 // 从 '?' 把 url 分割成两部分
			++ query_string;         // 指针指向 '?' 后的字符 
		 }
	}
	// 将 url 的前一部分拼接在字符串 "htdocs" 后面并存储到数组 path 中
	sprintf(path, "htdocs%s", url);
	// 如果 path 数组中的这个字符串以字符 '/' 结尾，则拼接上字符串 "index.html"
	if(path[strlen(path)-1] == '/')
		strcat(path, "index.html");
	// 根据路径找文件，并获取 path 文件信息保存到结构体 st 中
	if(stat(path, &st) == -1){
		// 如果不存在，把 http 请求的后续内容（head 和 body）全部读完并忽略
		while(numchars > 0 && strcmp("\n", buf))
			numchars = get_line(client, buf, sizeof(buf)); 
		not_found(client);  // 向客户端返回一个找不到文件的 response 
	} else {  // 获取文件信息，执行成功 
		if((st.st_mode & S_IFMT) == S_IFDIR)	// 如果该文件为目录类型 
			strcat(path, "/index.html");
		// 如果是可执行文件
		if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
			cgi = 1;
		if(!cgi)	// 静态页面请求
			serve_file(client, path);
		else	    // 动态页面请求 
			execute_cgi(client, path, method, query_string);	// 执行 cgi 脚本 
	}
	close(client);	  // 关闭客户端套接字
	return NULL;
}

/**
 * Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null)
 */
int get_line(int sock, char* buf, int size){
	int i = 0;
	char c = '\0';
	int n;
	while(i < size-1 && c != '\n'){
		n = recv(sock, &c, 1, 0);	// 读取 1 个字符放到 c 中 
		if(n > 0){
			if(c == '\r'){	  // 如果是回车符，则继续读取 
				// 使用 MSG_PEEK 标志是下一次读取依然可以得到这次读取的内容
				// 可认为输入队列窗口不滑动
				n = recv(sock, &c, 1, MSG_PEEK);
				if(n > 0 && c == '\n')	  // 回车+换行，读完后删除输入队列数据，即窗口滑动 
					recv(sock, &c, 1, 0);
				else	// 只读到回车符则置为换行符，终止读取 
					c = '\n';
			}
			buf[i++] = c;
		} else {	// 没有读取到任何数据 
			c = '\n';
		}
	}
	buf[i] = '\0';
	return i;
} // 回车、换行、回车+换行 三种情况均统一为换行 

/**
 * Inform the client that the requested web method has not been implemented.
 * Parameter: the client socket
 */
void unimplemented(int client){
	char buf[1024];
	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</TITLE></HEAD>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</P></BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/**
 * Give a client a 404 not found status message.
 */
void not_found(int client){
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</P></BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);	
}

/**
 * Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *             file descriptor
 *             the name of the file to serve 
 */
void serve_file(int client, const char* filename){
	FILE* resource = NULL;
	int numchars = 1;
	char buf[1024];
	// 确保 buf 中有内容，能进入下面的 while 循环 
	buf[0] = 'A';
	buf[1] = '\0';
	// 循环的作用是读取并忽略掉这个 http 请求后面的所有内容 
	while(numchars > 0 && strcmp("\n", buf))
		numchars = get_line(client, buf, sizeof(buf));
	// 以只读方式打开文件 
	resource = fopen(filename, "r");
	if(resource == NULL){
		not_found(client);
	} else {
		headers(client, filename);	// 将文件基本信息封装成 response 的 header 
		cat(client, resource);	// 把文件内容读出来作为 response 的 body 发送到客户端 
	}
	fclose(resource);
}

/**
 * Execute a CGI script. 
 * Parameters: client socket descriptor
 *             path to the CGI script 
 */
void execute_cgi(int client, const char* path, const char* method, const char* query_string){
	char buf[1024];
	int cgi_output[2];
	int cgi_input[2];
	pid_t pid;
	int status;
	int i;
	char c;
	int numchars = 1;
	int content_length = -1;
	// 往 buf 中填充内容，保证能够进入下面的 while 循环； 
	buf[0] = 'A';
	buf[1] = '\0';
	if(strcasecmp(method, "GET") == 0){	 // GET 方法 
		while(numchars > 0 && strcmp("\n", buf))
			numchars = get_line(client, buf, sizeof(buf));
	} else {  // POST 方法 
		numchars = get_line(client, buf, sizeof(buf));
		// 这个循环的目的是读出指示 body 长度大小的参数，其余的 header 参数一律忽略
		// 这里只是读完 header 的内容，body 的内容那个没有读 
		while(numchars > 0 && strcmp("\n", buf)){
			buf[15] = '\0';
			if(strcasecmp(buf, "Content-Length:") == 0)	 // 判断是否为 Content-Length 字段 
				content_length = atoi(&buf[16]);	//  Content-Length用于描述 HTTP 消息实体的长度 
			numchars = get_line(client, buf, sizeof(buf));
		}
		if(content_length == -1){
			bad_request(client);	// 请求的网页数据为空 
			return;
		}
	}
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	// 建立管道，cgi_output[0]: 读取端，cgi_output[1]: 写入端 
	if(pipe(cgi_output) < 0){
		cannot_execute(client);	 // 建立管道失败，打印出错信息 
		return;
	}
	if(pipe(cgi_input) < 0){
		cannot_execute(client);
		return;
	}
	// 管道只能在公共祖先的进程间进行，这里是父子进程之间
	// 创建子进程
	if((pid = fork()) < 0){
		cannot_execute(client);
		return;
	}
	// 子进程用来执行 CGI 脚本
	// 实现进程间的管道通信机制
	if(pid == 0){	// 子进程
		char meth_env[255];
		char query_env[255];
		char length_env[255];
		// 重定向进程的标准输入输出
		dup2(cgi_output[1], 1);	 // 标准输出重定向到 output 管道的写入端 
		dup2(cgi_input[0], 0);   // 标准输入重定向到 input 管道的读取端 
		close(cgi_output[0]);    // 关闭 output 管道的读取端 
		close(cgi_input[1]);     // 关闭 input 管道的写入端 
		sprintf(meth_env, "REQUEST_METHOD=%s", method);  // 构造一个环境变量 
		putenv(meth_env);	 // 将环境变量加进子进程的运行环境中
		// 根据 http 请求的不同方法，构造并存储不同的环境变量 
		if(strcasecmp(method, "GET") == 0){
			sprintf(query_env, "QUERY_STRING=%s", query_string);
			putenv(query_env);
		} else {  // POST
			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
			putenv(length_env);
		}
		// exec 函数族，执行 CGI 脚本，获取 CGI 的标准输出作为相应内容发送给客户端
		// 通过 dup2 重定向，标准输出内容进入 output 管道的输入端 
		execl(path, path, NULL);
		exit(0);	// 子进程退出 
	} else {	// 父进程 
		// 父进程关闭 cgi_output 的写端和 cgi_input 的读端
		close(cgi_output[1]);
		close(cgi_input[0]);
		// 如果是 POST 方法就继续读 body 的内容
		if(strcasecmp(method, "POST") == 0){
			for(i=0; i<content_length; ++i){
				recv(client, &c, 1, 0);		// 从客户端接收单个字符 
				write(cgi_input[1], &c, 1); // 写入 input管道，处理结果从 output[0] 输出 
			}
		}
		// 读取 ouput 的输出端，发送到客户端
		// 如果不是 POST 方法，则只有这一处理 
		while(read(cgi_output[0], &c, 1) > 0)
			send(client, &c, 1, 0);
		// 关闭剩下的管道端 
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);	// 等待子进程终止 
	}
}

/**
 * Return the informational HTTP headers about a file.
 * Parameters: the socket to print the headers on
 *             the name of the file 
 */
void headers(int client, const char* filename){
 	char buf[1024];
 	(void)filename;  // 可以用文件名确定文件类型
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);	   	
}
 
/**
 * Put the entire contents of a file out on a socket.
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat 
 */
void cat(int client, FILE* resource){
	char buf[1024];
	fgets(buf, sizeof(buf), resource);
	while(!feof(resource)){
		send(client, buf, strlen(buf), 0);
		fgets(buf, sizeof(buf), resource);
	}
}

/**
 * Inform the client that a request it has made has a problem.
 * Parameters: client socket
 */
void bad_request(int client){
	char buf[1024];
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**
 * Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor
 */
void cannot_execute(int client){
	char buf[1024];
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

