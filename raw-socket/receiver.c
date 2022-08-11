#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "include/public/bier.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <Listening UNIX socket> <IPv6 multicast listening address> <BIER daemon socket>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    struct sockaddr_un addr = {};
    int socket_fd;
    int err;

    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // This socket will listen to incoming packets from the BIER daemon
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, argv[1]);

    // https://medium.com/swlh/getting-started-with-unix-domain-sockets-4472c0db4eb1
    if (remove(argv[1]) == -1 && errno != ENOENT) {
        perror("Remove unix socket path");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Bind the UNIX socket for communication with the BIER daemon
    err = bind(socket_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
    if (err == -1) {
        perror("Bind unix socket");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Bound to UNIX socket\n");

    // Destination is UNIX socket running the BIER process
    struct sockaddr_un dst = {};  
    dst.sun_family = AF_UNIX;
    strcpy(dst.sun_path, argv[3]);
    int data_len = strlen(dst.sun_path) + sizeof(dst.sun_family);

    bier_bind_t bier_bind = {};
    strcpy(bier_bind.unix_path, argv[1]);
    bier_bind.proto = BIERPROTO_IPV6;
    struct sockaddr_in6 mc_group = {
        .sin6_family = AF_INET6,
    };
    if (inet_pton(AF_INET6, argv[2], &mc_group.sin6_addr.s6_addr) == 0) {
        perror("IPv6 MC destination");
        exit(EXIT_FAILURE);
    }
    memcpy(&bier_bind.mc_sockaddr, &mc_group, sizeof(struct sockaddr_in6));

    // "Bind" to IPv6 multicast address
    if (bind_bier(socket_fd, &dst, &bier_bind) < 0) {
        fprintf(stderr, "Confirmed\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Bound to multicast address %s\n", argv[2]);

    // Dummy listener: listen for packets and output the content on the standard
    // output
    uint8_t packet[4096];
    struct sockaddr src_received = {};
    char src_received_txt[150];
    socklen_t addrlen;
    bier_info_t bier_info;
    while (1) {
        ssize_t received = recvfrom_bier(socket_fd, packet, sizeof(packet),
                                         &src_received, &addrlen, &bier_info);
        fprintf(stderr, "Received %lu bytes from %s\n", received,
                src_received_txt);
        if (received < 0) {
            perror("received bier");
            break;
        }
        if (inet_ntop(AF_INET6, &src_received, src_received_txt, addrlen) ==
            NULL) {
            perror("inet ntop");
            break;
        }
        // fprintf(stderr, "The ID of the router that sent the packet: %ld\n", bier_info.recv_info.upstream_router_bfr_id);
        fprintf(stderr, "First few bytes are: ");
        for (int i = 0; i < 10; ++i) {
            fprintf(stderr, "%x ", packet[i]);
        }
        fprintf(stderr, "\n");
    }

    close(socket_fd);
}