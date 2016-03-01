#ifndef __ERROR_H__
#define __ERROR_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <syslog.h>
void unix_error(char* msg);

void posix_error(int code, char* msg);

void gai_error(int code, char* msg);

void app_error(char* msg);

void dns_error(char* msg);

#endif
