#ifndef _LIB_H_
#define _LIB_H_

#include <rio.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LISTENQ  1024  /* second argument to listen() */
#define MAXLINE 1024   /* max length of a line */

/* Simplifies calls to bind(), connect(), and accept() */
typedef struct sockaddr SA;

struct {
    char* method;
    char* filename;
    int start_byte;
    int end_byte;
} http_request;

int open_listenfd(int port);
ssize_t writen(int fd, void *usrbuf, size_t n);
ssize_t readn(int fd, void *usrbuf, size_t n);
void process(int fd);


#endif /* _LIB_H_ */

