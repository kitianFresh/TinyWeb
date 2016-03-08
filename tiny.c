#include "rio/wrapper.h"
#include "sbuf.h"

#define SBUFSIZE 32
#define LOCKFILE "/home/kitian/csapp/Tiny_MultiThread/run/tiny.pid"
#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

#define MAX_PORTNUM 16
#define MAX_PATH 256
#define CONF_PATH "/home/kitian/csapp/Tiny_MultiThread/conf/conf"
#define DELIM "="
struct config {
	char port[MAX_PORTNUM];
	int nthreads;
	char logpath[MAX_PATH];
	int fdnum;
} conf;

void getconf(char *filename);
int lockfile(int fd);
int already_runnig(void);
void daemonize(const char *name);
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
	int logfd, listenfd, connfd, clientlen, i;
	struct sockaddr_in clientaddr;
	pthread_t tid;
	struct hostent *hp;
	char *haddrp;
	char hostname[MAXLINE], portclient[MAXLINE];

	
	
	/* become a daemon */
	daemonize("tiny");
	
	Signal(SIGCHLD, handler); /* Register signal to handle child process */
	Signal(SIGPIPE, SIG_IGN); /* Ignore SIGPIPE to avoid process exit*/
	
	/* Make sure only one copy of the daemon is running */
	if (already_running()) {
		syslog(LOG_DAEMON|LOG_ERR, "Tiny already running");
		exit(1);
	}
	getconf(CONF_PATH);
	//logfd = Open("/home/kitian/csapp/Tiny_MultiThread/weblog/logs.txt", 
	//	O_APPEND | O_CREAT | O_RDWR, DEF_MODE & ~DEF_UMASK);
	
	sbuf_init(&sbuf, SBUFSIZE);
	listenfd = Open_listenfd(conf.port);
	clientlen = sizeof(clientaddr);
	
	syslog(LOG_DAEMON|LOG_INFO, "Tiny started at port %s\n\n", conf.port);
	for (i = 0; i < conf.nthreads; i++) {	/* Create worker threads */
		Pthread_create(&tid, NULL, thread, NULL);
		syslog(LOG_DAEMON|LOG_INFO, "Tiny create thread %ld\n", (unsigned long)tid);
	}

	while(1){
		connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
		Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    portclient, MAXLINE, 0);
        	syslog(LOG_DAEMON|LOG_INFO,"Accepted connection from (%s, %s)\n", hostname, portclient);
		sbuf_insert(&sbuf, connfd);
	}
}

void getconf(char *filename){
	FILE *file = fopen(filename,"r");
	if (NULL != file) {
		char line[MAXBUF], *cfline, *index;
		int len;
		while (fgets(line, sizeof(line), file) != NULL){
			cfline = line;
			/* Skip leading whitespace */
			while (isspace(*cfline)) {
		    	cfline++;
			}

			/* Ignore if there is character '#' front */
			if ('#' == *cfline || '\0' == *cfline) continue;
			
			index = strstr((char*)cfline,DELIM);
			*index = '\0';
			index ++;
			len = strlen(index);
			if (*(index+len-1) == '\n') /* remove char '\n' at the end */
				*(index+len-1) = '\0';
			if (!strcasecmp(cfline, "port"))
				strncpy(conf.port,index,strlen(index));
			if (!strcasecmp(cfline, "nthreads"))
				conf.nthreads = atoi(index);
			if (!strcasecmp(cfline, "logpath"))
				strncpy(conf.logpath,index, strlen(index));
			if (!strcasecmp(cfline, "fdnum"))
				conf.fdnum = atoi(index);
		}
		return;

	}
	else {
		syslog(LOG_DAEMON|LOG_ERR,"open configuration file failed\n");
		exit(1);
	}
}

int lockfile(int fd){
	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	return (fcntl(fd, F_SETLK, &fl));
}

int already_running(void){
	int fd;
	char buf[16];
	fd = Open(LOCKFILE, O_RDWR|O_CREAT, LOCKMODE);
	if (lockfile(fd) < 0) { /* Check if locked */
		if (errno == EACCES || errno == EAGAIN) {
			close(fd);
			return 1;
		}
		syslog(LOG_DAEMON|LOG_ERR,"can't lock %s: %s", LOCKFILE, strerror(errno));
		exit(1);
	}
	ftruncate(fd, 0); /* truncate to clear old content */
	sprintf(buf,"%ld",(long)getpid());
	Write(fd,buf,strlen(buf)+1);
	return 0;
}

void daemonize(const char *name){
	int i, fd0, fd1, fd2;
	pid_t pid;
	struct rlimit rl;

	/* Clear file creation mask */
	umask(0);

	/* Get maximum number of file descriptors */
	Getrlimit(RLIMIT_NOFILE, &rl);

	/* Become a session leader to lose controlling TTY */
	if ((pid = Fork()) != 0) exit(0); /* Parent exit */
	setsid();

	/* Ensure future opens won't allocate contorlling TTYs */
	Signal(SIGHUP, SIG_IGN);
	if ((pid = Fork()) != 0) exit(0); /* Parent exit*/

	/* Change the current working directory to the root so 
	 * we won't prevent file systems from being unmounted.
	 */
	Chdir("/home/kitian/csapp/Tiny_MultiThread");

	/* Close all open file descriptors. because 0 1 2 opened at least */
	if (RLIM_INFINITY == rl.rlim_max)
		rl.rlim_max = 1024;
	for (i = 0; i < rl.rlim_max; i++)
		close(i);

	/* Attach file descriptors 0 1 and 2 to /dev/null */
	fd0 = open("/dev/null", O_RDWR);
	fd1 = Dup(0);
	fd2 = Dup(0);

	/* Initialize the log file. */
	openlog(name, LOG_CONS | LOG_PID, LOG_DAEMON);
	if (fd0 !=0 || fd1 != 1 || fd2 != 2){
		syslog(LOG_ERR, "unexpected file descriptors %d %d %d",
				fd0, fd1, fd2);
		exit(1);
	}
}

void *thread(void *vargp){
	int connfd;
	Pthread_detach(Pthread_self());/* detach itself from main thread */
	while(1) { 
		connfd = sbuf_remove(&sbuf);/* Remove connfd from buffer */
		syslog(LOG_DAEMON|LOG_INFO, "Thread %ld dispose connfd %d\n",(unsigned long)Pthread_self(),connfd);
		doit(connfd);				/* Service client */
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
	syslog(LOG_DAEMON|LOG_INFO, "Request Line:%s", buf);
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
		syslog(LOG_DAEMON|LOG_INFO, "serve_static finished\n");
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

	while(strcmp(buf, "\r\n")){
		Rio_readlineb(rp, buf, MAXLINE);
		syslog(LOG_DAEMON|LOG_INFO, "%s", buf);
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
	sprintf(buf, "%sServer: Tiny Web Server by kikifly,static\r\n", buf);
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
	sprintf(buf, "Server: Tiny Web Server by kikifly,dynamic\r\n");
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
	sprintf(body, "%s<hr><em>The Tiny Web server By kikifly,clienterror</em>\r\n", 
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
		syslog(LOG_DAEMON|LOG_INFO,"Handler reaped child %d\n", (int)pid);
		if (WIFEXITED(status))
			syslog(LOG_DAEMON|LOG_INFO,"child %d terminated normally with exit status=%d\n",pid,WEXITSTATUS(status));
		else
			syslog(LOG_DAEMON|LOG_INFO,"child %d terminated abnormally\n",pid);
	}
	
	return;
}
