#include "rio/wrapper.h"
#include "sbuf.h"
#define NTHREADS 8
#define SBUFSIZE 32

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void handler(int sig);
void *thread(void *vargp); /* Thread routine*/

sbuf_t sbuf; /*Shared buffer of connected descriptors */

int main(int argc, char **argv){
	int logfd, listenfd, connfd, port, clientlen, i;
	struct sockaddr_in clientaddr;
	pthread_t tid;
	struct hostent *hp;
	char *haddrp;
	
	Signal(SIGCHLD, handler); /* Register signal to handle child process */
	Signal(SIGPIPE, SIG_IGN); /* Ignore SIGPIPE to avoid process exit*/
	/* Check command line args */
	if(2 != argc){
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	port = atoi(argv[1]);
	logfd = Open("./weblog/logs.txt", 
		O_APPEND | O_CREAT | O_RDWR, DEF_MODE & ~DEF_UMASK);
	Dup2(logfd,STDOUT_FILENO);
	
	sbuf_init(&sbuf, SBUFSIZE);
	listenfd = Open_listenfd(port);
	clientlen = sizeof(clientaddr);
	
	fprintf(stdout, "Tiny started at port %d\n\n", port);
	for (i = 0; i < NTHREADS; i++) {	/* Create worker threads */
		Pthread_create(&tid, NULL, thread, NULL);
		fprintf(stdout, "Tiny create thread %ld\n", (unsigned long)tid);
	}
	fflush(stdout);
	while(1){
		connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
		sbuf_insert(&sbuf, connfd);
	}
}
void *thread(void *vargp){
	int connfd;
	Pthread_detach(Pthread_self());/* detach itself from main thread */
	while(1) { 
		connfd = sbuf_remove(&sbuf);/* Remove connfd from buffer */
		fprintf(stdout, "Thread %ld dispose connfd %d\n",(unsigned long)Pthread_self(),connfd);
		doit(connfd);				/* Service client */
		fflush(stdout);
		Close(connfd);				/* Clear client fd */
	}
}
void doit(int fd){
	int is_static;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio;

	/* Read request line and headers */
	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
	if (strcasecmp(method, "GET")) {
		clienterror(fd, method, "501", "Not Implemented", "Tiny has not implemented this method yet, you can tell kikifly");
		return;
	}
	fprintf(stdout, "%s", buf);
	read_requesthdrs(&rio);

	/* Parse URI from GET request */
	is_static = parse_uri(uri, filename, cgiargs);
	if (stat(filename, &sbuf) < 0) {
		clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file, you can tell kikifly");
		return;
	}
	
	if (is_static) {
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file, you can tell kikifly");
			return;
		}
		serve_static(fd, filename, sbuf.st_size);
	}
	else {
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program, you can tell kikifly");
			return;
		}
		serve_dynamic(fd,filename,cgiargs);
	}
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
