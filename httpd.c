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

#define ISspace(x) isspace((int)(x))	// �� x Ϊ�ո��ַ������� true 
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void* accept_request(void*);	        // ������׽����ϼ������� HTTP ���� 
void  bad_request(int);	                // ���ظ��ͻ��ˣ����Ǹ���������HTTP ״̬�� 400 BAD REQUEST 
void  cat(int, FILE*);                  // ��ȡ��������ĳ���ļ���д���׽��� 
void  cannot_execute(int);              // ������ִ�� cgi ����ʱ���ֵĴ��� 
void  error_die(const char*);           // �Ѵ�����Ϣд�� perror ���˳� 
void  execute_cgi(int, const char*, const char*, const char*);	  // ���� cgi �ű� 
int   get_line(int, char*, int);        // ��ȡ�׽��ֵ�һ�У��ѻس����е����ͳһΪ�Ի��з����� 
void  headers(int, const char*);        // �� HTTP ��Ӧ���ĵ�ͷ��д���׽��� 
void  not_found(int);                   // �����Ҳ��������ļ�ʱ����� 
void  serve_file(int, const char*);     // ���� cat���ѷ������ļ����ظ������ 
int   startup(u_short*);               // ��ʼ�� HTTP ���񣬰��������׽��֡��󶨶˿ڡ����м����� 
void  unimplemented(int);               // ���ظ���������յ��� HTTP �������õ� method ����֧�� 

int main(){
	int server_sock = -1;
	u_short port = 0;
	int client_sock = -1;
	struct sockaddr_in client_name;    // �׽��ֵ�ַ�ṹ�������� <netinet/in.h>
	socklen_t client_name_len = sizeof(client_name);
	pthread_t newthread;
	server_sock = startup(&port);
	printf("Tinyhttpd running on port %d\n", port);
	// ���̲߳���������ģ��
	while(1){
		// �����ȴ��ͻ��˵����� 
		// ���߳� 
		client_sock = accept(server_sock, (struct sockaddr*)&client_name, &client_name_len);
		if(client_sock == -1)
			error_die("accept");
		// ���������̣߳�ִ�лص����� accept_request������Ϊ client_sock  
		if(pthread_create(&newthread, NULL, accept_request, (void*)&client_sock) != 0)
			perror("pthread_create");
	}
	close(server_sock);	   // �ر��׽��֣���Э��ջ�Ƕȿ������ر� TCP ���� 
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
	// socket() ���ڴ���һ�� socket ������������������ <sys/socket.h> �� 
	// PF_INET �� AF_INET ͬ�� 
	httpd = socket(PF_INET, SOCK_STREAM, 0);
	if(httpd == -1)
		error_die("socket");
	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;	  // ��ַ��
	// ָ���˿�
	// �� *port ת�����������ֽ����ʾ��16λ���� 
	name.sin_port = htons(*port);
	// INADDR_ANY ��һ�� IPv4 ͨ���ַ����
	// �����ʵ�ֽ��䶨��� 0.0.0.0  
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	// bind() ���ڰ󶨵�ַ�� socket
	// �������ȥ�� sin_port Ϊ0����ʱϵͳ��ѡ��һ����ʱ�Ķ˿ں� 
	if(bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0)
		error_die("bind");
	// ������� bind() ��˿ں���ȻΪ0�����ֶ����� getsockname() ��ȡ�˿ں� 
	if(*port == 0){
		socklen_t namelen = sizeof(name);
		if(getsockname(httpd, (struct sockaddr*)&name, &namelen) == -1)
			error_die("getsockname");
		*port = ntohs(name.sin_port); // �����ֽ���ת��Ϊ�����ֽ��� 
	}
	// �����������ͻ��������׽����Ŷӵ�������Ӹ���Ϊ5 
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
	char method[255];	// ���󷽷� GET or POST
	char url[255];	    // ������ļ�·��
	char path[512];     // �ļ����·��
	size_t i, j;
	struct stat st;
	int cgi = 0;		//����� CGI ������ cgi Ϊ true
	char* query_string = NULL;
	numchars = get_line(client, buf, sizeof(buf));  //�� client �ж�ȡָ����С���ݵ� buf
	i = 0, j = 0;
	// ���� http ������
	// �����ַ�������ȡ�ո�ǰ���ַ�������254��
	while(!ISspace(buf[j]) && (i < sizeof(method)-1))
		method[i++] = buf[j++];
	method[i] = '\0';
	// ���Դ�Сд�Ƚ��ַ������������ GET �� POST ���κ�һ���� 
	// ��ֱ�ӷ��� response ���߿ͻ���ûʵ�ָ÷���
	if(strcasecmp(method, "GET") && strcasecmp(method, "POST")){
		unimplemented(client);
		return NULL;
	}
	// ����� POST �������Ͱ� cgi ��־λ��Ϊ true
	if(strcasecmp(method, "POST") == 0)
		cgi = 1;
	i = 0;
	// ���˵��ո��ַ����ո������ url
	while(ISspace(buf[j]) && (j < sizeof(buf)))
		++ j;
	// �������еķǿո��ַ�ת��� url �����������ո��ַ������˳�
	while(!ISspace(buf[j]) && (i < sizeof(url)-1) && (j < sizeof(buf)))
		url[i++] = buf[j++];
	url[i] = '\0';
	if(strcasecmp(method, "GET") == 0){	   // GET ���� 
		query_string = url;
		// ��ȡ '?' ǰ���ַ�������������Ҳû�ҵ��ַ� '?'�����˳�ѭ��
		while((*query_string != '?') && (*query_string != '\0'))
			++ query_string;
		if(*query_string == '?'){
			cgi = 1;	             // ����� '?'��˵�����������Ҫ���� cgi
			*query_string = '\0'; 	 // �� '?' �� url �ָ��������
			++ query_string;         // ָ��ָ�� '?' ����ַ� 
		 }
	}
	// �� url ��ǰһ����ƴ�����ַ��� "htdocs" ���沢�洢������ path ��
	sprintf(path, "htdocs%s", url);
	// ��� path �����е�����ַ������ַ� '/' ��β����ƴ�����ַ��� "index.html"
	if(path[strlen(path)-1] == '/')
		strcat(path, "index.html");
	// ����·�����ļ�������ȡ path �ļ���Ϣ���浽�ṹ�� st ��
	if(stat(path, &st) == -1){
		// ��������ڣ��� http ����ĺ������ݣ�head �� body��ȫ�����겢����
		while(numchars > 0 && strcmp("\n", buf))
			numchars = get_line(client, buf, sizeof(buf)); 
		not_found(client);  // ��ͻ��˷���һ���Ҳ����ļ��� response 
	} else {  // ��ȡ�ļ���Ϣ��ִ�гɹ� 
		if((st.st_mode & S_IFMT) == S_IFDIR)	// ������ļ�ΪĿ¼���� 
			strcat(path, "/index.html");
		// ����ǿ�ִ���ļ�
		if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
			cgi = 1;
		if(!cgi)	// ��̬ҳ������
			serve_file(client, path);
		else	    // ��̬ҳ������ 
			execute_cgi(client, path, method, query_string);	// ִ�� cgi �ű� 
	}
	close(client);	  // �رտͻ����׽���
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
		n = recv(sock, &c, 1, 0);	// ��ȡ 1 ���ַ��ŵ� c �� 
		if(n > 0){
			if(c == '\r'){	  // ����ǻس������������ȡ 
				// ʹ�� MSG_PEEK ��־����һ�ζ�ȡ��Ȼ���Եõ���ζ�ȡ������
				// ����Ϊ������д��ڲ�����
				n = recv(sock, &c, 1, MSG_PEEK);
				if(n > 0 && c == '\n')	  // �س�+���У������ɾ������������ݣ������ڻ��� 
					recv(sock, &c, 1, 0);
				else	// ֻ�����س�������Ϊ���з�����ֹ��ȡ 
					c = '\n';
			}
			buf[i++] = c;
		} else {	// û�ж�ȡ���κ����� 
			c = '\n';
		}
	}
	buf[i] = '\0';
	return i;
} // �س������С��س�+���� ���������ͳһΪ���� 

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
	// ȷ�� buf �������ݣ��ܽ�������� while ѭ�� 
	buf[0] = 'A';
	buf[1] = '\0';
	// ѭ���������Ƕ�ȡ�����Ե���� http ���������������� 
	while(numchars > 0 && strcmp("\n", buf))
		numchars = get_line(client, buf, sizeof(buf));
	// ��ֻ����ʽ���ļ� 
	resource = fopen(filename, "r");
	if(resource == NULL){
		not_found(client);
	} else {
		headers(client, filename);	// ���ļ�������Ϣ��װ�� response �� header 
		cat(client, resource);	// ���ļ����ݶ�������Ϊ response �� body ���͵��ͻ��� 
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
	// �� buf ��������ݣ���֤�ܹ���������� while ѭ���� 
	buf[0] = 'A';
	buf[1] = '\0';
	if(strcasecmp(method, "GET") == 0){	 // GET ���� 
		while(numchars > 0 && strcmp("\n", buf))
			numchars = get_line(client, buf, sizeof(buf));
	} else {  // POST ���� 
		numchars = get_line(client, buf, sizeof(buf));
		// ���ѭ����Ŀ���Ƕ���ָʾ body ���ȴ�С�Ĳ���������� header ����һ�ɺ���
		// ����ֻ�Ƕ��� header �����ݣ�body �������Ǹ�û�ж� 
		while(numchars > 0 && strcmp("\n", buf)){
			buf[15] = '\0';
			if(strcasecmp(buf, "Content-Length:") == 0)	 // �ж��Ƿ�Ϊ Content-Length �ֶ� 
				content_length = atoi(&buf[16]);	//  Content-Length�������� HTTP ��Ϣʵ��ĳ��� 
			numchars = get_line(client, buf, sizeof(buf));
		}
		if(content_length == -1){
			bad_request(client);	// �������ҳ����Ϊ�� 
			return;
		}
	}
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	// �����ܵ���cgi_output[0]: ��ȡ�ˣ�cgi_output[1]: д��� 
	if(pipe(cgi_output) < 0){
		cannot_execute(client);	 // �����ܵ�ʧ�ܣ���ӡ������Ϣ 
		return;
	}
	if(pipe(cgi_input) < 0){
		cannot_execute(client);
		return;
	}
	// �ܵ�ֻ���ڹ������ȵĽ��̼���У������Ǹ��ӽ���֮��
	// �����ӽ���
	if((pid = fork()) < 0){
		cannot_execute(client);
		return;
	}
	// �ӽ�������ִ�� CGI �ű�
	// ʵ�ֽ��̼�Ĺܵ�ͨ�Ż���
	if(pid == 0){	// �ӽ���
		char meth_env[255];
		char query_env[255];
		char length_env[255];
		// �ض�����̵ı�׼�������
		dup2(cgi_output[1], 1);	 // ��׼����ض��� output �ܵ���д��� 
		dup2(cgi_input[0], 0);   // ��׼�����ض��� input �ܵ��Ķ�ȡ�� 
		close(cgi_output[0]);    // �ر� output �ܵ��Ķ�ȡ�� 
		close(cgi_input[1]);     // �ر� input �ܵ���д��� 
		sprintf(meth_env, "REQUEST_METHOD=%s", method);  // ����һ���������� 
		putenv(meth_env);	 // �����������ӽ��ӽ��̵����л�����
		// ���� http ����Ĳ�ͬ���������첢�洢��ͬ�Ļ������� 
		if(strcasecmp(method, "GET") == 0){
			sprintf(query_env, "QUERY_STRING=%s", query_string);
			putenv(query_env);
		} else {  // POST
			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
			putenv(length_env);
		}
		// exec �����壬ִ�� CGI �ű�����ȡ CGI �ı�׼�����Ϊ��Ӧ���ݷ��͸��ͻ���
		// ͨ�� dup2 �ض��򣬱�׼������ݽ��� output �ܵ�������� 
		execl(path, path, NULL);
		exit(0);	// �ӽ����˳� 
	} else {	// ������ 
		// �����̹ر� cgi_output ��д�˺� cgi_input �Ķ���
		close(cgi_output[1]);
		close(cgi_input[0]);
		// ����� POST �����ͼ����� body ������
		if(strcasecmp(method, "POST") == 0){
			for(i=0; i<content_length; ++i){
				recv(client, &c, 1, 0);		// �ӿͻ��˽��յ����ַ� 
				write(cgi_input[1], &c, 1); // д�� input�ܵ����������� output[0] ��� 
			}
		}
		// ��ȡ ouput ������ˣ����͵��ͻ���
		// ������� POST ��������ֻ����һ���� 
		while(read(cgi_output[0], &c, 1) > 0)
			send(client, &c, 1, 0);
		// �ر�ʣ�µĹܵ��� 
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid, &status, 0);	// �ȴ��ӽ�����ֹ 
	}
}

/**
 * Return the informational HTTP headers about a file.
 * Parameters: the socket to print the headers on
 *             the name of the file 
 */
void headers(int client, const char* filename){
 	char buf[1024];
 	(void)filename;  // �������ļ���ȷ���ļ�����
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

