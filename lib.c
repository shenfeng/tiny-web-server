#include "lib.h"

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

static void client_error(int fd, char *errornum, char *msg) {
    char buf[MAXLINE];
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errornum, msg);
    sprintf(buf, "%sContent-length: 0\r\n\r\n", buf);
    writen(fd, buf, strlen(buf));
}

static void server_static(int out_fd, int in_fd, long size) {
    char buf[128];
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %lu\r\n", buf, size);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, "text/plain");
    writen(out_fd, buf, strlen(buf));

    long begin = 0;
    sendfile(out_fd, in_fd, &begin, size);
}

void process(int fd) {
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
    }

    struct stat sbuf;
    int ffd = open(filename, O_RDONLY, 0);
    if(ffd <= 0) {
        client_error(fd, "404", "Not found");
        return;
    }
    fstat(ffd, &sbuf);

    if(S_ISREG(sbuf.st_mode)) {
        server_static(fd, ffd ,sbuf.st_size);
    } else {
        client_error(fd, "403", "Error");
    }
    close(ffd);
}
