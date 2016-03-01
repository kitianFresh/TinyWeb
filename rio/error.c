#include "error.h"
/************************** 
 *  * Error-handling functions
 *   **************************/

void unix_error(char *msg) /* Unix-style error */
{
	syslog(LOG_DAEMON|LOG_ERR, "%s: %s\n", msg, strerror(errno));
	exit(0);
}
/* $end unixerror */

void posix_error(int code, char *msg) /* Posix-style error */
{
	syslog(LOG_DAEMON|LOG_ERR, "%s: %s\n", msg, strerror(code));
	exit(0);
}

void gai_error(int code, char *msg) /* Getaddrinfo-style error */
{
	syslog(LOG_DAEMON|LOG_ERR, "%s: %s\n", msg, gai_strerror(code));
	exit(0);
}

void app_error(char *msg) /* Application error */
{
	syslog(LOG_DAEMON|LOG_ERR, "%s\n", msg);
	exit(0);
}
/* $end errorfuns */

void dns_error(char *msg) /* Obsolete gethostbyname error */
{
	syslog(LOG_DAEMON|LOG_ERR, "%s\n", msg);
	exit(0);
}
