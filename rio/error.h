#ifndef ERROR_H
#define ERROR_H

void unix_error(char* msg);

void posix_error(int code, char* msg);

void gai_error(int code, char* msg);

void app_error(char* msg);

void dns_error(char* msg);

#endif
