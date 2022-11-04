#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <sys/time.h>
#include <string.h>
#include <syslog.h>

void p(char *s) {
    fprintf(stderr, "%s\n", s);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        p("args: <src> <dst> <nb>");
        exit(1);
    }

    int sock_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        p("socket");
        exit(1);
    }

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        p("setsockopt");
        exit(1);
    }

    struct sockaddr_in6 src = {};
    src.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, argv[1], src.sin6_addr.s6_addr) < 0) {
        p("addr src");
        exit(1);
    }

    struct sockaddr_in6 dst = {};
    dst.sin6_family = AF_INET6;
    dst.sin6_port = 8765;
    if (inet_pton(AF_INET6, argv[2], dst.sin6_addr.s6_addr) < 0) {
        p("addr dst");
        exit(1);
    }

    int nb = atoi(argv[3]);
    if (nb == 0) {
        p("nb");
        exit(1);
    }
    
    if (bind(sock_fd, (struct sockaddr *)&src, sizeof(src)) < 0) {
        p("bind");
        exit(1);
    }

    char buff[1000] = {};
    for (int i = 0; i < nb; ++i) {
        ssize_t r = sendto(sock_fd, buff, sizeof(buff), 0, (struct sockaddr *)&dst, sizeof(dst));
        if (r < 0) {
            p("recvfrom");
            exit(1);
        }
        printf("Send a packet\n");
	    // sleep(1);
    }

    close(sock_fd);
}
