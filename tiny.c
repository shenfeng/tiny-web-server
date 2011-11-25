#include <rio.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>          /* inet_ntoa */
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
void process(int fd, struct sockaddr_in *clientaddr);
void client_error(int fd, int status, char *msg, char *longmsg) ;
void server_static(int out_fd, int in_fd, long size) ;
void log_access(int status, size_t size,
                struct sockaddr_in *clientaddr, char*filename);


int main(int argc, char** argv) {

    struct sockaddr_in clientaddr;

    int default_port = 9999,
        listenfd,
        clientlen = sizeof clientaddr,
        connfd;
    if(argc == 2) {
        default_port = atoi(argv[1]);
    }

    listenfd = open_listenfd(default_port);
    if (listenfd > 0){
        printf("listen on port %d, fd is %d\n", default_port, listenfd);
    } else {
        fprintf(stderr,"error listen on port %d, error code %d",
                default_port, listenfd);
        exit(listenfd);
    }

    while(1) {
        connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        process(connfd, &clientaddr);
        close(connfd);
    }

    return 0;
}


int open_listenfd(int port){
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
}


void process(int fd, struct sockaddr_in *clientaddr) {
    rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];

    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);

    sscanf(buf, "%s %s %s", method, uri, version);

    /* read all */
    while(buf[0] != '\n' && buf[1] != '\n') { /* \n || \r\n */
        rio_readlineb(&rio, buf, MAXLINE);
    }
    char* filename = uri;
    if(uri[0] == '/') {
        filename = uri + 1;
        if (strlen(filename) == 0){
            filename = ".";
        }
    }

    struct stat sbuf;
    int status = 200, size = 0;
    int ffd = open(filename, O_RDONLY, 0);
    if(ffd <= 0) {
        status = 404;
        char *msg = "File not found";
        client_error(fd, status, "Not found", msg);
        size = strlen(msg);
    } else {
        fstat(ffd, &sbuf);
        if(S_ISREG(sbuf.st_mode)) {
            size = sbuf.st_size;
            server_static(fd, ffd ,sbuf.st_size);
        } else if(S_ISDIR(sbuf.st_mode)) {
            status = 200;
            char *msg = "Directory listing is not implemented yet";
            size = strlen(msg);
            client_error(fd, status, "OK", msg);
        } else {
            status = 400;
            char *msg = "Unknow Error";
            size = strlen(msg);
            client_error(fd, status, "Error", msg);
        }
    }
    log_access(status, size, clientaddr, filename);
    close(ffd);
}

void log_access(int status, size_t size, struct sockaddr_in *clientaddr,
                char *filename) {
    printf("%s:%d %d - %d %s\n", inet_ntoa(clientaddr->sin_addr),
           ntohs(clientaddr->sin_port), status, size, filename);
}

void client_error(int fd, int status, char *msg, char *longmsg) {
    char buf[MAXLINE];
    sprintf(buf, "HTTP/1.1 %d %s\r\n", status, msg);
    sprintf(buf, "%sContent-length: %lu\r\n\r\n", buf, strlen(longmsg));
    sprintf(buf, "%s%s", buf, longmsg);
    writen(fd, buf, strlen(buf));
}

void server_static(int out_fd, int in_fd, long size) {
    char buf[128];
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    /* unsigned long: %lu */
    sprintf(buf, "%sContent-length: %lu\r\n", buf, size);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, "text/plain");
    writen(out_fd, buf, strlen(buf));

    long begin = 0;
    sendfile(out_fd, in_fd, &begin, size);
}
