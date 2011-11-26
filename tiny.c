#include <arpa/inet.h>          /* inet_ntoa */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <rio.h>
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

typedef struct {
    const char *extension;
    const char *mime_type;
} mime_map;

mime_map meme_types [] = {
    {".css", "text/css"},
    {".gif", "image/gif"},
    {".gz", "application/x-gunzip"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".js", "application/javascript"},
    {".mp4", "video/mp4"},
    {".mp3", "audio/x-mp3"},
    {".exe", "application/octet-stream"},
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".xml", "text/xml"},
    {NULL, NULL},
};

char *default_mime_type = "text/plain";

int open_listenfd(int port);
void process(int fd, struct sockaddr_in *clientaddr);
void client_error(int fd, int status, char *msg, char *longmsg) ;
void serve_static(int out_fd, int in_fd, char* filename, long size) ;
void log_access(int status, size_t size,
                struct sockaddr_in *clientaddr, char*filename);

void handle_directory_request(int out_fd, int dir_fd, char *filename) {
    char buf[MAXLINE];
    struct stat statbuf;
    sprintf(buf, "HTTP/1.1 200 OK\r\n%s\r\n",
            "Content-Type: text/html\r\n\r\n");
    writen(out_fd, buf, strlen(buf));
    DIR *d = fdopendir(dir_fd);
    struct dirent *dp;
    int ffd;
    while ((dp = readdir(d)) != NULL) {
        if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
            continue;
        }
        if ((ffd = openat(dir_fd, dp->d_name, O_RDONLY)) == -1) {
            perror(dp->d_name);
            continue;
        }
        if (fstat(ffd, &statbuf) == 0) {
            if(S_ISREG(statbuf.st_mode)) {
                sprintf(buf, "<p><a href=\"%s\">%s</a></p>",
                        dp->d_name, dp->d_name);
            } else if (S_ISDIR(statbuf.st_mode)) {
                sprintf(buf, "<p><a href=\"%s/\">%s</a></p>",
                        dp->d_name, dp->d_name);
            } else {
                continue;
            }
            writen(out_fd, buf, strlen(buf));
        }
    }
    closedir(d);
}

static const char* get_mime_type(char *filename) {
    char *dot = strrchr(filename, '.');
    if(dot) { // strrchar Locate last occurrence of character in string
        mime_map *map = meme_types;
        while(map->extension) {
            if(strcmp(map->extension, dot) == 0) {
                return map->mime_type;
            }
            map++;
        }
    }
    return default_mime_type;
}


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

    // 6 is TCP's protocol number
    // enable this, much faster : 4000 req/s -> 17000 req/s
    if (setsockopt(listenfd, 6, TCP_CORK,
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
            serve_static(fd, ffd, filename, sbuf.st_size);
        } else if(S_ISDIR(sbuf.st_mode)) {
            status = 200;
            handle_directory_request(fd, ffd, filename);
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
    printf("%s:%d %d - %lu %s\n", inet_ntoa(clientaddr->sin_addr),
           ntohs(clientaddr->sin_port), status, size, filename);
}

void client_error(int fd, int status, char *msg, char *longmsg) {
    char buf[MAXLINE];
    sprintf(buf, "HTTP/1.1 %d %s\r\n", status, msg);
    sprintf(buf + strlen(buf),
            "Content-length: %lu\r\n\r\n", strlen(longmsg));
    sprintf(buf + strlen(buf), "%s", longmsg);
    writen(fd, buf, strlen(buf));
}

void serve_static(int out_fd, int in_fd, char* filename, long size) {
    char buf[128];
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    /* unsigned long: %lu */
    sprintf(buf + strlen(buf), "Content-length: %lu\r\n", size);
    sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n",
            get_mime_type(filename));
    writen(out_fd, buf, strlen(buf));

    long begin = 0;
    sendfile(out_fd, in_fd, &begin, size);
}
