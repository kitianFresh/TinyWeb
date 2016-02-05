#include "error.h"
#include "rio.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
/*********************************************
 *  * Wrappers for Unix process control functions
 *   ********************************************/

/* $begin forkwrapper */
pid_t Fork(void) 
{
	pid_t pid;
	if ((pid = fork()) < 0)
		unix_error("Fork error");
	return pid;
}
/* $end forkwrapper */

void Execve(const char *filename, char *const argv[], char *const envp[]) 
{
	if (execve(filename, argv, envp) < 0)
		unix_error("Execve error");
}

/* $begin wait */
pid_t Wait(int *status) 
{
	pid_t pid;
	if ((pid  = wait(status)) < 0)
		unix_error("Wait error");
	return pid;
}
/* $end wait */

pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
	pid_t retpid;
	if ((retpid  = waitpid(pid, iptr, options)) < 0) 
		unix_error("Waitpid error");
	return(retpid);
}

/* $begin kill */
void Kill(pid_t pid, int signum) 
{
	int rc;
	if ((rc = kill(pid, signum)) < 0)
		unix_error("Kill error");
}
/* $end kill */

void Pause() 
{
	(void)pause();
	return;
}

unsigned int Sleep(unsigned int secs) 
{
	return sleep(secs);
}

unsigned int Alarm(unsigned int seconds) {
	return alarm(seconds);
}
 
void Setpgid(pid_t pid, pid_t pgid) {
	int rc;
	if ((rc = setpgid(pid, pgid)) < 0)
		unix_error("Setpgid error");
	return;
}

pid_t Getpgrp(void) {
	return getpgrp();
}

/**********************************
 *  * Wrappers for robust I/O routines
 *   **********************************/
ssize_t Rio_readn(int fd, void *ptr, size_t nbytes) 
{
	ssize_t n;	  
	if ((n = rio_readn(fd, ptr, nbytes)) < 0)
		unix_error("Rio_readn error");
	return n;
}

void Rio_writen(int fd, void *usrbuf, size_t n) 
{
	if (rio_writen(fd, usrbuf, n) != n)
		unix_error("Rio_writen error");
}

void Rio_readinitb(rio_t *rp, int fd)
{
	rio_readinitb(rp, fd);
} 

ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
	ssize_t rc;
	if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
		unix_error("Rio_readnb error");
	return rc;
}

ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;
	if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
		unix_error("Rio_readlineb error");
		return rc;
}

/********************************
 * Wrappers for Unix I/O routines
 ********************************/

int Open(const char *pathname, int flags, mode_t mode) 
{
    int rc;

    if ((rc = open(pathname, flags, mode))  < 0)
	unix_error("Open error");
    return rc;
}

ssize_t Read(int fd, void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = read(fd, buf, count)) < 0) 
	unix_error("Read error");
    return rc;
}

ssize_t Write(int fd, const void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = write(fd, buf, count)) < 0)
	unix_error("Write error");
    return rc;
}

off_t Lseek(int fildes, off_t offset, int whence) 
{
    off_t rc;

    if ((rc = lseek(fildes, offset, whence)) < 0)
	unix_error("Lseek error");
    return rc;
}

void Close(int fd) 
{
    int rc;

    if ((rc = close(fd)) < 0)
	unix_error("Close error");
}

int Select(int  n, fd_set *readfds, fd_set *writefds,
	   fd_set *exceptfds, struct timeval *timeout) 
{
    int rc;

    if ((rc = select(n, readfds, writefds, exceptfds, timeout)) < 0)
	unix_error("Select error");
    return rc;
}

int Dup2(int fd1, int fd2) 
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
	unix_error("Dup2 error");
    return rc;
}

void Stat(const char *filename, struct stat *buf) 
{
    if (stat(filename, buf) < 0)
	unix_error("Stat error");
}

void Fstat(int fd, struct stat *buf) 
{
    if (fstat(fd, buf) < 0)
	unix_error("Fstat error");
}

/*********************************
 * Wrappers for directory function
 *********************************/

DIR *Opendir(const char *name) 
{
    DIR *dirp = opendir(name); 

    if (!dirp)
        unix_error("opendir error");
    return dirp;
}

struct dirent *Readdir(DIR *dirp)
{
    struct dirent *dep;
    
    errno = 0;
    dep = readdir(dirp);
    if ((dep == NULL) && (errno != 0))
        unix_error("readdir error");
    return dep;
}

int Closedir(DIR *dirp) 
{
    int rc;

    if ((rc = closedir(dirp)) < 0)
        unix_error("closedir error");
    return rc;
}

/***************************************
 * Wrappers for memory mapping functions
 ***************************************/
void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) 
{
    void *ptr;

    if ((ptr = mmap(addr, len, prot, flags, fd, offset)) == ((void *) -1))
	unix_error("mmap error");
    return(ptr);
}

void Munmap(void *start, size_t length) 
{
    if (munmap(start, length) < 0)
	unix_error("munmap error");
}

/***************************************************
 * Wrappers for dynamic storage allocation functions
 ***************************************************/

void *Malloc(size_t size) 
{
    void *p;

    if ((p  = malloc(size)) == NULL)
	unix_error("Malloc error");
    return p;
}

void *Realloc(void *ptr, size_t size) 
{
    void *p;

    if ((p  = realloc(ptr, size)) == NULL)
	unix_error("Realloc error");
    return p;
}

void *Calloc(size_t nmemb, size_t size) 
{
    void *p;

    if ((p = calloc(nmemb, size)) == NULL)
	unix_error("Calloc error");
    return p;
}

void Free(void *ptr) 
{
    free(ptr);
}

/******************************************
 * Wrappers for the Standard I/O functions.
 ******************************************/
void Fclose(FILE *fp) 
{
    if (fclose(fp) != 0)
	unix_error("Fclose error");
}

FILE *Fdopen(int fd, const char *type) 
{
    FILE *fp;

    if ((fp = fdopen(fd, type)) == NULL)
	unix_error("Fdopen error");

    return fp;
}

char *Fgets(char *ptr, int n, FILE *stream) 
{
    char *rptr;

    if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
	app_error("Fgets error");

    return rptr;
}

FILE *Fopen(const char *filename, const char *mode) 
{
    FILE *fp;

    if ((fp = fopen(filename, mode)) == NULL)
	unix_error("Fopen error");

    return fp;
}

void Fputs(const char *ptr, FILE *stream) 
{
    if (fputs(ptr, stream) == EOF)
	unix_error("Fputs error");
}

size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    size_t n;

    if (((n = fread(ptr, size, nmemb, stream)) < nmemb) && ferror(stream)) 
	unix_error("Fread error");
    return n;
}

void Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    if (fwrite(ptr, size, nmemb, stream) < nmemb)
	unix_error("Fwrite error");
}

