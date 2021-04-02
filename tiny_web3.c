#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef struct sockaddr SA;

//struct sockaddr {
//　　unsigned short sa_family; /* address family, AF_xxx */
//　　char sa_data[14]; /* 14 bytes of protocol address */
//　　};

#define RIO_BUFSIZE 8192
typedef struct {
	int rio_fd;                /* Descriptor for this internal buf */
	int rio_cnt;               /* Unread bytes in internal buf */
	char *rio_bufptr;          /* Next unread byte in internal buf */
	char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
} rio_t;

// 定义范围
#define	MAXLINE	 8192  /* Max text line length */
#define MAXBUF   8192  /* Max I/O buffer size */
#define LISTENQ  1024  /* Second argument to listen() */

void doit(int fd); // // 处理HTTP事务
void read_requestthdrs(rio_t *rp);   // 读取并忽略请求报头
int parse_uri(char* uri, char *filename, char *cgiargs);  // 解析一个HTTP URI
void serve_static(int fd, char *filename, int filesize);  // 为客户端提供静态内容
void get_filetype(char *filename, char *filetype);  // 获得文件类型
void serve_dynamic(int fd, char *filename, char *cgiargs);  // 为客户端提供动态内容
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);  // 服务器错误处理
ssize_t rio_writen(int fd, void *usrbuf, size_t n); 
void rio_readinitb(rio_t *rp, int fg);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);

extern char **environ; /* Defined by libc */

int main(int argc, char **argv)
{
	int listenfd, connfd, port, clientlen;
	struct sockaddr_in clientaddr;

	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		return(1);
	}
	port = atoi(argv[1]);
	
	//初始化，开始监听
	listenfd = open_listenfd(port);

	while (1)
	{
		clientlen = sizeof(clientaddr);
		connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
		doit(connfd);  // 处理HTTP事务
		close(connfd);
	}
}

// 服务器初始化
int open_listenfd(int port)
{
	int listenfd, optval = 1;
	struct sockaddr_in serveraddr;
	
	// 创建套接字描述符
	// AF_INET 表明使用因特网
	// SOCK_STREAM表示这个套接字是因特网连接的一个端点
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	//使用setsockopt配置服务器，能被立即终止和启动
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
		return -1;

	// 清空
	bzero((char *)&serveraddr, sizeof(serveraddr));
	// 配置ip和端口， 转换成网络字节顺序
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); //32位整数由主机字节顺序转换为网络字节顺序
	serveraddr.sin_port = htons((unsigned short)port); //16位整数由主机字节顺序转换为网络字节顺序
	if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
		return -1;

	if (listen(listenfd, LISTENQ) < 0)
		return -1;
	return listenfd;
}

// 处理HTTP事务
void doit(int fd)
{
	int is_static;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio;

	rio_readinitb(&rio, fd);
	// 读和解析请求行
	rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
	// 判断请求类型
	if (strcasecmp(method, "GET"))
	{
		clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
		return;
	}
	read_requestthdrs(&rio);   // 读取并忽略请求报头

	// 将URI解析为一个文件名和一个可能为空的CGI参数字符串
	// is_static表明为静态还是动态
	is_static = parse_uri(uri, filename, cgiargs);
	// 文件不存在
	if (stat(filename, &sbuf) < 0)
	{
		clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
		return;
	}

	// 判断是否有权限
	if (is_static)
	{	
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) //S_ISREG是否是一个常规文件，S_IRUSR所有者拥有读权限
		{
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");  // 服务器错误处理
			return;
		}
		serve_static(fd, filename, sbuf.st_size);   // 为客户端提供静态内容
	}
	else
	{
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) //S_ISREG是否是一个常规文件，S_IXUSR所有者拥有执行权限
		{
			clienterror(fd, filename, "403", "Forbidden", "Tiny coudln't run the CGI program");  // 服务器错误处理
			return;
		}
		serve_dynamic(fd, filename, cgiargs);   // 为客户端提供动态内容
	}
}

// 服务器错误处理
void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg)
{
	char buf[MAXLINE], body[MAXBUF];

	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type:text/html\r\n");
	rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	rio_writen(fd, buf, strlen(buf));
	rio_writen(fd, buf, strlen(body));
}

// 读取并忽略请求报头
void read_requestthdrs(rio_t *rp)
{
	char buf[MAXLINE];

	rio_readlineb(rp, buf, MAXLINE);
	while (strcmp(buf, "\r\n"))
	{
		rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
	}
}

// 解析一个HTTP URI
int parse_uri(char* uri, char* filename, char *cgiargs)
{
	char *ptr;

	if (!strstr(uri, "cgi-bin"))
	{
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		if (uri[strlen(uri) - 1] == '/')
			strcat(filename, "home.html");
		return 1;
	}
	else
	{
		ptr = index(uri, '?');
		if (ptr)
		{
			strcpy(cgiargs, ptr + 1);
			*ptr = '\0';
		}
		else
			strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcpy(filename, uri);
		return 0;
	}
}

// 为客户端提供静态内容
void serve_static(int fd, char* filename, int filesize)
{
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];

	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	rio_writen(fd, buf, strlen(buf));

	srcfd = open(filename, O_RDONLY, 0);
	srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	close(srcfd);
	rio_writen(fd, srcp, filesize);
	munmap(srcp, filesize);
}

// 获得文件类型
void get_filetype(char* filename, char *filetype)
{
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
	else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else
		strcpy(filetype, "text/plain");
}

// 为客户端提供动态内容
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char buf[MAXLINE], *emptylist[] = { NULL };

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Tiny Web Server\r\n");
	rio_writen(fd, buf, strlen(buf));

	if (fork() == 0)
	{
		setenv("QUERY_STRING", cgiargs, 1);
		dup2(fd, STDOUT_FILENO);      // 复制文件描述符
		execve(filename, emptylist, environ);
	}
	wait(NULL);
}

ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
	size_t nleft = n;
	ssize_t nwritten;
	char *bufp = usrbuf;

	while (nleft > 0)
	{
		if ((nwritten = write(fd, bufp, nleft)) <= 0)
		{
			if (errno == EINTR)
				nwritten = 0;
			else
				return -1;
		}
		nleft -= nwritten;
		bufp += nwritten;
	}
	return n;
}

void rio_readinitb(rio_t *rp, int fd)
{
	rp->rio_fd = fd;
	rp->rio_cnt = 0;
	rp->rio_bufptr = rp->rio_buf;
}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
	int n, rc;
	char c, *bufp = usrbuf;

	for (n = 1; n < maxlen; n++)
	{
		if ((rc = rio_read(rp, &c, 1)) == 1)
		{
			*bufp++ = c;
			if (c == '\n')
				break;
		}
		else if (rc == 0)
		{
			if (n == 1)
				return 0;
			else
				break;
		}
		else
			return -1;
	}
	*bufp = 0;
	return n;
}

static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
	int cnt;

	while (rp->rio_cnt <= 0)
	{
		rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));

		if (rp->rio_cnt < 0)
		{
			if (errno != EINTR)
				return -1;
		}
		else if (rp->rio_cnt == 0)
			return 0;
		else
			rp->rio_bufptr = rp->rio_buf;

	}

	cnt = n;
	if (rp->rio_cnt < n)
		cnt = rp->rio_cnt;
	memcpy(usrbuf, rp->rio_bufptr, cnt);
	rp->rio_bufptr += cnt;
	rp->rio_cnt -= cnt;
	return cnt;
}