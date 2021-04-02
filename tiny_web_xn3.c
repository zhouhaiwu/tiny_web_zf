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
  
#define BUFLEN 1024 
#define PORT 6666
#define LISTNUM 20
#define RIO_BUFSIZE 8192

// 定义范围
#define	MAXLINE	 8192  /* Max text line length */
#define MAXBUF   8192  /* Max I/O buffer size */
#define LISTENQ  1024  /* Second argument to listen() */
typedef struct {
	int rio_fd;                /* Descriptor for this internal buf */
	int rio_cnt;               /* Unread bytes in internal buf */
	char *rio_bufptr;          /* Next unread byte in internal buf */
	char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
} rio_t;

int open_listenfd(int port);
void read_requestthdrs(rio_t *rp);   // 读取并忽略请求报头
int parse_uri(char* uri, char *filename, char *cgiargs);  // 解析一个HTTP URI
void get_filetype(char *filename, char *filetype);  // 获得文件类型
void clienterror(char *cause, char *errnum, char *shortmsg, char *longmsg);  // 服务器错误处理
ssize_t rio_writen(int fd, void *usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fg);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);

int main() 
{ 
    int sockfd, newfd; 
    struct sockaddr_in s_addr, c_addr; 
    char buf[BUFLEN]; 
    socklen_t len; 
    unsigned int port, listnum; 
    fd_set rfds; 
    struct timeval tv; 
    int retval,maxfd; 
      
    /*建立socket*/ 
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1){ 
        perror("socket"); 
        exit(errno); 
    }else 
        printf("socket create success!\n"); 
    memset(&s_addr,0,sizeof(s_addr)); 
    s_addr.sin_family = AF_INET; 
    s_addr.sin_port = htons(PORT); 
    s_addr.sin_addr.s_addr = htons(INADDR_ANY); 
    
    /*把地址和端口帮定到套接字上*/ 
    if((bind(sockfd, (struct sockaddr*) &s_addr,sizeof(struct sockaddr))) == -1){ 
        perror("bind"); 
        exit(errno); 
    }else 
        printf("bind success!\n"); 
    /*侦听本地端口*/ 
    if(listen(sockfd,listnum) == -1){ 
        perror("listen"); 
        exit(errno); 
    }else 
        printf("the server is listening!\n"); 
    while(1){ 
        printf("*****************聊天开始***************\n"); 
        len = sizeof(struct sockaddr); 
        if((newfd = accept(sockfd,(struct sockaddr*) &c_addr, &len)) == -1){ 
            perror("accept"); 
            exit(errno); 
        }else 
            printf("正在转发您信息的客户端是：%s: %d\n",inet_ntoa(c_addr.sin_addr),ntohs(c_addr.sin_port)); 
        while(1){ 

            /******接收消息*******/ 
            memset(buf,0,sizeof(buf)); 
            /*fgets函数：从流中读取BUFLEN-1个字符*/ 
            len = recv(newfd,buf,BUFLEN,0);
            if(len > 0) 
                printf("客户端发来的信息是：%s\n",buf); 
            else{ 
                if(len < 0 ) 
                    printf("接受消息失败！\n"); 
                else 
                    printf("客户端退出了，聊天终止！\n"); 
                break; 
            }
		    //web服务器程序
		    int is_static;
		    struct stat sbuf;
		    //char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    	    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
		    char filename[MAXLINE], cgiargs[MAXLINE];
		    rio_t rio;
		    printf("buf:%s\n", buf);
		    sscanf(buf, "%s %s %s", method, uri, version); //
		    printf("method:%s\n", method);
		    printf("uri:%s\n", uri);
		    printf("version:%s\n", version);
		    // 判断请求类型
		    if (strcasecmp(method, "GET"))
		    {
    	        printf("Tiny does not implement this method\n");
		    	//clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
		    	return -1;
		    }
		    //printf("read_requestthdrs\n");
		    //read_requestthdrs(&rio);   // 读取并忽略请求报头
		    // 将URI解析为一个文件名和一个可能为空的CGI参数字符串
		    // is_static表明为静态还是动态
		    is_static = parse_uri(uri, filename, cgiargs);
		    printf("filename:%s\n", filename);
		    printf("is_static:%d\n", is_static);
		    printf("****************\n");
		    // 文件不存在
		    if (stat(filename, &sbuf) < 0)
		    {
		    	printf("haha\n");
		    	printf("Tiny couldn't find this file\n");
		    	//clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
		    	return -1;
		    }
		    // 判断是否有权限
		    if (is_static)
		    {
		    	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) //S_ISREG是否是一个常规文件，S_IRUSR所有者拥有读权限
		    	{
		    		printf("Tiny couldn't read the file");
		    		//clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");  // 服务器错误处理
		    		return -1;
		    	}
		    	// 为客户端提供静态内容
		    	int srcfd;
		    	int size;
		    	//char *srcp, filetype[MAXLINE], buf[MAXBUF], buf_open[MAXBUF];
		    	char *srcp, filetype[MAXLINE], buf[MAXBUF];
		    	srcfd = open(filename, O_RDONLY, 0);
		    	//srcp = mmap(0, MAXBUF, PROT_READ, MAP_PRIVATE, srcfd, 0); //会去下载文件 (C:\Users\zhou1\Downloads)
		    	//close(srcfd);  //  为什么不是web服务器主目录下面
		    	//len = send(newfd, srcp, MAXBUF, 0);
		    	size = read(srcfd, buf, sbuf.st_size);
		    	if (size <0) {
		    		printf("read static file failed!\n");
		    	}
		    	else {
		    		printf("read static file success!\n");
		    	}
		    	close(srcfd);
		    	len = send(newfd,buf,strlen(buf),0);
		    	//serve_static(fd, filename, sbuf.st_size);   // 为客户端提供静态内容
		    }
		    else
		    {
		    	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) //S_ISREG是否是一个常规文件，S_IXUSR所有者拥有执行权限
		    	{
		    		printf("Tiny coudln't run the CGI program");
		    		//clienterror(fd, filename, "403", "Forbidden", "Tiny coudln't run the CGI program");  // 服务器错误处理
		    		return -1;
		    	}
		    	//serve_dynamic(fd, filename, cgiargs);   // 为客户端提供动态内容
		    }
            len = send(newfd,buf,strlen(buf),0);
            if(len > 0) 
                printf("\t消息发送成功：%s\n",buf); 
            else{ 
                printf("消息发送失败!\n"); 
                break; 
                } 
            }
            //close(newfd); 
        }
    /*关闭服务器的套接字*/ 
    close(newfd);
    close(sockfd); 
    return 0; 
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
		//strcpy(filename, "/root/C/TINY");
		strcat(filename, uri);
		if (uri[strlen(uri) - 1] == '/')
			strcat(filename, "home.html");
		printf("filename1:%s\n", filename);
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
		printf("filename2:%s\n", filename);
		return 0;
	}
	//printf("filename:%s\n", filename);
}

// 获得文件类型
void get_filetype(char* filename, char *filetype)
{
	if (strstr(filename, ".html")) {
		strcpy(filetype, "text/html");
		printf("filetype\n");
	}
	else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else
		strcpy(filetype, "text/plain");
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