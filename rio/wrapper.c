#include "error.h"
#include "rio.h"
#include<unistd.h>
#include<sys/types.h>
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
