#include <stdio.h>
#include <unistd.h>
#include "../wrapper.h"
#define MAXLINE 8192

int main(int argc, char **argv){
	int n,fd;
	rio_t rio;
	char buf[MAXLINE];
	if(argc == 2){
		fd = Open(argv[1], O_RDONLY,0);
		Rio_readinitb(&rio,fd);
	}
	else
		Rio_readinitb(&rio,STDIN_FILENO);
	
	while((n = Rio_readlineb(&rio,buf,MAXLINE)) != 0){
		Rio_writen(STDOUT_FILENO,buf,n);
	}
}
