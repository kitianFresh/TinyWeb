#include "rio/wrapper.h"

int doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void handler(int sig);


/******************************
 *使用select实现的IO多路复用
 ******************************/
typedef struct {
	int maxfd;			/* Largest descriptor in read_set */
	fd_set read_set;	/* Set of all active desciptors */
	fd_set ready_set;	/* Subset of descriptors ready for reading */
	int nready;			/* Number of ready descriptors from select */
	int maxi;			/* Highwater index into client array */
	int clientfd[FD_SETSIZE];	/* Set of active descriptors */
	rio_t clientrio[FD_SETSIZE];/* Set of active read buffers */
} pool;
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_client(pool *p);

int main(int argc, char **argv){
	int logfd, listenfd, connfd, port, clientlen, childpid=0;
	struct sockaddr_in clientaddr;
	
	struct hostent *hp;
	char *haddrp;
	static pool pool;
	
	Signal(SIGCHLD, handler); /* Register signal to handle child process */
	/* Check command line args */
	if(2 != argc){
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	port = atoi(argv[1]);
	logfd = Open("./weblog/logs.txt", 
		O_APPEND | O_CREAT | O_RDWR, DEF_MODE & ~DEF_UMASK);
	Dup2(logfd,STDOUT_FILENO);

	listenfd = Open_listenfd(port);
	init_pool(listenfd, &pool);
	clientlen = sizeof(clientaddr);
	//fprintf(stdout, "Tiny started at port %d\n\n", port);
	while(1){
		/* Wait for listening/connected descriptor to become ready */
		pool.ready_set = pool.read_set;
		pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
		if (FD_ISSET(listenfd, &pool.ready_set)) {
			connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
			fprintf(stdout, "Tiny Accepted and got connfd: %d\n", connfd);
			hp = Gethostbyaddr((const char*)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
			haddrp = inet_ntoa(clientaddr.sin_addr);
			fprintf(stdout, "Tiny connected to %s (%s)\n", hp->h_name, haddrp);
			add_client(connfd, &pool);
		}

		check_client(&pool);
	}
}

int doit(int fd){
	int is_static, n;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio;

	/* Read request line and headers */
	Rio_readinitb(&rio, fd);
	if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
		sscanf(buf, "%s %s %s", method, uri, version);
		if (strcasecmp(method, "GET")) {
			clienterror(fd, method, "501", "Not Implemented", "Tiny has not implemented this method yet, you can tell kikifly");
			return -1;
		}
		fprintf(stdout, "%s", buf);
		read_requesthdrs(&rio);

		/* Parse URI from GET request */
		is_static = parse_uri(uri, filename, cgiargs);
		if (stat(filename, &sbuf) < 0) {
			clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file, you can tell kikifly");
			return -1;
		}
	
		if (is_static) {
			if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
				clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file, you can tell kikifly");
				return -1;
			}
			serve_static(fd, filename, sbuf.st_size);
		}
		else {
			if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
				clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program, you can tell kikifly");
				return -1;
			}
			serve_dynamic(fd,filename,cgiargs);
		}
	}
	else {
		return 0; /* connfd EOF detected,, return 0*/
	}
	return n;
}

void read_requesthdrs(rio_t *rp){
	char buf[MAXLINE];

	//fprintf(stdout, "Tiny read request header:\n");
	while(strcmp(buf, "\r\n")){
		Rio_readlineb(rp, buf, MAXLINE);
		fprintf(stdout, "%s", buf);
	}
	return;
}

int parse_uri(char *uri, char *filename, char *cgiargs){
	char *ptr;

	if (!strstr(uri, "cgi-bin")){ /* Static content */
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		if (uri[strlen(uri)-1] == '/') /* Is default page */
			strcat(filename, "resources/text/home.html");
		return 1;
	}
	else {
		ptr = index(uri, '?');
		if (ptr) { /* args not null */
			strcpy(cgiargs, ptr+1);
			*ptr = '\0'; /* truncate the uri from '?' after fetching cgiargs*/
		}
		else
			strcpy(cgiargs, ""); /* No args*/

		strcpy(filename, ".");
		strcat(filename, uri);
		return 0;
	}
}

void serve_static(int fd, char *filename, int filesize){
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];

	/* Send response line and headers to client */
	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.1 200 OK\r\n");
	sprintf(buf, "%sServer: Tiny Web Server by kikifly\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));

	/* Send response body to client */
	srcfd = Open(filename, O_RDONLY, 0);
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	Close(srcfd);
	Rio_writen(fd, srcp, filesize);
	Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype){
	if (strstr(filename, ".html"))
		strcpy(filetype, "text/html;charset=utf-8");
	else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
	else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
	else if (strstr(filename, ".mp4"))
		strcpy(filetype, "video/mpeg4");
	else
		strcpy(filetype, "text/plain;charset=utf-8");
}

void serve_dynamic(int fd, char *filename, char *cgiargs){
	char buf[MAXLINE], *emptylist[] = {NULL};

	/* Return first part of HTTP response */
	sprintf(buf, "HTTP/1.1 200 OK\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Tiny Web Server by kikifly\r\n");
	Rio_writen(fd, buf, strlen(buf));
	
	
	if (Fork() == 0) {
		/* Real server would set all CGI vars here */
		setenv("QUERY_STRING", cgiargs, 1);
		Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
		Execve(filename, emptylist, environ); /* Run CGI program */
		//exit(0);
	}
	//Wait(NULL);  /*Parent waits for and reaps child */
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
	char buf[MAXLINE], body[MAXBUF];

	/* Build the HTTP response body */
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ff0000"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server By kikifly</em>\r\n", 
			body);

	/*Print the HTTP response */
	sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}

void handler(int sig){
	pid_t pid;
	int status;
	while ((pid = Waitpid(-1,&status,WNOHANG | WUNTRACED)) > 0){
		printf("Handler reaped child %d\n", (int)pid);
		if (WIFEXITED(status))
			printf("child %d terminated normally with exit status=%d\n",pid,WEXITSTATUS(status));
		else
			printf("child %d terminated abnormally\n",pid);
	}
	
	//Sleep(2);
	return;
}

void init_pool(int listenfd, pool *p){
	/* Initially, there are no connected descriptors */
	int i;
	p->maxi = -1;
	for (i=0; i < FD_SETSIZE; i++)
		p->clientfd[i] = -1;

	/* Initially, listenfd is only member of select read set */
	p->maxfd = listenfd;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p){
	int i;
	p->nready--;
	for (i=0; i < FD_SETSIZE; i++) /* Find an available slot */
		if (p->clientfd[i] < 0) {
			/* Add connected descriptor to the pool */
			p->clientfd[i] = connfd;
			Rio_readinitb(&p->clientrio[i], connfd);

			/* Add the descriptor to desciptor set  */
			FD_SET(connfd, &p->read_set);

			/* Update max descriptor and pool highwater mark */
			if (connfd > p->maxfd)
				p->maxfd = connfd;
			if ( i > p->maxi)
				p->maxi = i;
			break;
		}
	if(i == FD_SETSIZE) /* Couldn't find an empty slot */
		app_error("add_client error: Too many clients");
}

void check_client(pool *p){
	int i, connfd, n;
	rio_t rio;

	for (i=0; (i <= p->maxi) && (p->nready > 0); i++) {
		connfd = p->clientfd[i];
		rio = p->clientrio[i];

		/* If the descriptor is ready, doit */
		if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
			p->nready--;
			if ((n = doit(connfd)) == 0) {
				Close(connfd);
				FD_CLR(connfd, &p->read_set);
				p->clientfd[i] = -1;
			}
			fflush(stdout);
		}
	}
}
