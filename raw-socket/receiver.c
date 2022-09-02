#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <getopt.h>

#include "include/public/bier.h"

typedef struct {
    char listening_unix_path[NAME_MAX];
    char mc_addr[NAME_MAX];
    char bier_unix_path[NAME_MAX];
    int nb_packets_listen;
} args_t;

void parse_args(args_t *args, int argc, char *argv[]) {
    memset(args, 0, sizeof(args_t));
    int opt;
    bool has_listening_unix_path, has_mc_addr, has_bier_unix_path;
    args->nb_packets_listen = 1;

    while ((opt = getopt(argc, argv, "l:g:b:n:")) != -1) {
        switch (opt) {
            case 'l': {
                strcpy(args->listening_unix_path, optarg);
                has_listening_unix_path = true;
                break;
            }
            case 'g': {
                strcpy(args->mc_addr, optarg);
                has_mc_addr = true;
                break;
            }
            case 'b': {
                strcpy(args->bier_unix_path, optarg);
                has_bier_unix_path = true;
                break;
            }
            case 'n': {
                int nb = atoi(optarg);
                if (nb == 0) {
                    fprintf(stderr, "Cannot convert to int: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                args->nb_packets_listen = nb;
                break;
            }
            default: {
                fprintf(stderr, "TODO default usage\n");
                break;
            }
        }
    }

    if (!(has_listening_unix_path && has_mc_addr && has_bier_unix_path)) {
        fprintf(stderr, "Should provide all the arguments\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    args_t args;
    parse_args(&args, argc, argv);

    struct sockaddr_un addr = {};
    int socket_fd;
    int err;

    socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // This socket is only used to communicate with the BIER daemon
    // DCE does not like to use a bound socket to do that
    int socket_to_bier = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_to_bier < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // This socket will listen to incoming packets from the BIER daemon
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, args.listening_unix_path);

    // https://medium.com/swlh/getting-started-with-unix-domain-sockets-4472c0db4eb1
    if (remove(args.listening_unix_path) == -1 && errno != ENOENT) {
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
    strcpy(dst.sun_path, args.bier_unix_path);
    int data_len = strlen(dst.sun_path) + sizeof(dst.sun_family);

    bier_bind_t bier_bind = {};
    strcpy(bier_bind.unix_path, args.listening_unix_path);
    bier_bind.proto = BIERPROTO_IPV6;
    struct sockaddr_in6 mc_group = {
        .sin6_family = AF_INET6,
    };
    if (inet_pton(AF_INET6, args.mc_addr, &mc_group.sin6_addr.s6_addr) == 0) {
        perror("IPv6 MC destination");
        exit(EXIT_FAILURE);
    }
    memcpy(&bier_bind.mc_sockaddr, &mc_group, sizeof(struct sockaddr_in6));

    // "Bind" to IPv6 multicast address
    if (bind_bier(socket_to_bier, &dst, &bier_bind) < 0) {
        fprintf(stderr, "Confirmed\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Bound to multicast address %s\n", args.mc_addr);

    // Dummy listener: listen for packets and output the content on the standard
    // output
    uint8_t packet[4096];
    struct sockaddr src_received = {};
    char src_received_txt[150];
    socklen_t addrlen;
    bier_info_t bier_info;
    int nb_received = 0;
    while (nb_received < args.nb_packets_listen) {
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
        ++nb_received;
    }

    fprintf(stderr, "Received %d packets... Closing the program\n", nb_received);
    if (unbind_bier(socket_to_bier, &dst, &bier_bind) < 0) {
        fprintf(stderr, "Confirmed2\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Send a LEAVE message\n");

    close(socket_fd);
    close(socket_to_bier);
}