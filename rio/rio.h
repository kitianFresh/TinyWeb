#ifndef __RIO_H__
#define __RIO_H__
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>

#define RIO_BUFSIZE 8192
typedef struct {
	int rio_fd;                 /*Descriptor for this internal buf*/
	int rio_cnt;	            /*Unread bytes in internal buf*/
	char* rio_bufptr;           /*Next unread byte in internal buf*/
	char rio_buf[RIO_BUFSIZE];  /*Internal buffer*/
} rio_t;

/****************************************
 *  * The Rio package - Robust I/O functions
 *   ****************************************/
/*No bufering*/
ssize_t rio_readn(int fd, void* usrbuf, size_t n);
ssize_t rio_writen(int fd, void* usrbuf, size_t n);


/*IO with buffering*/
void rio_readinitb(rio_t* rp, int fd);

ssize_t rio_readlineb(rio_t* rp, void* usrbuf, size_t maxlen);
ssize_t rio_readnb(rio_t* rp, void* usrbuf, size_t n);
static ssize_t rio_read(rio_t* rp, char* usrbuf, size_t n);
#endif
