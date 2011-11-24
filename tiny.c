#include <lib.h>

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
        process(connfd);
        close(connfd);
    }

    return 0;
}

