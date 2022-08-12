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

#include "include/public/bier.h"
#include "include/public/multicast.h"

bool verbose;

my_packet_t *dummy_packet(char *mc_dst_addr) {
    // Destination of the multicast packet embedded in the BIER packet
    // This must be a multicast address
    char *destination_address = mc_dst_addr;
    struct sockaddr_in6 mc_dst = {};

    int err =
        inet_pton(AF_INET6, destination_address, &mc_dst.sin6_addr.s6_addr);
    if (err != 1) {
        fprintf(stderr, "%s\n", mc_dst_addr);
        perror("IPv6 destination");
        exit(EXIT_FAILURE);
    }

    // Destination of the multicast packet embedded in the BIER packet
    // This must be a multicast address
    char *source_address = "babe::1";
    struct sockaddr_in6 mc_src = {};

    err = inet_pton(AF_INET6, source_address, &mc_src.sin6_addr.s6_addr);
    if (err != 1) {
        perror("IPv6 source");
        exit(EXIT_FAILURE);
    }

    uint8_t payload[1000];
    memset(payload, 0, sizeof(payload));
    payload[999] = 1;
    my_packet_t *packet =
        create_ipv6_from_payload(&mc_src, &mc_dst, sizeof(payload), payload);

    return packet;
}

typedef struct {
    char mc_dst[INET6_ADDRSTRLEN];
    char loopback[INET6_ADDRSTRLEN];
    char bier_daemon_path[NAME_MAX];
    char sender_path[NAME_MAX];
    int nb_packets_to_send;
    int bift_id;
    bool verbose;
} args_t;

void parse_args(args_t *args, int argc, char *argv[]) {
    memset(args, 0, sizeof(args_t));
    int opt;
    bool has_mc_dst, has_loopback, has_bier, has_sender;
    args->nb_packets_to_send = 1;
    args->bift_id = 1;
    args->verbose = false;

    while ((opt = getopt(argc, argv, "d:l:b:s:n:i:v")) != -1) {
        switch (opt) {
            case 'v': {
                args->verbose = true;
                break;
            }
            case 'd': {
                strcpy(args->mc_dst, optarg);
                has_mc_dst = true;
                break;
            }
            case 'l': {
                strcpy(args->loopback, optarg);
                has_loopback = true;
                break;
            }
            case 'b': {
                strcpy(args->bier_daemon_path, optarg);
                has_bier = true;
                break;
            }
            case 's': {
                strcpy(args->sender_path, optarg);
                has_sender = true;
                break;
            }
            case 'n': {
                int nb = atoi(optarg);
                if (nb == 0) {
                    perror("Cannot parse nb packets to send");
                } else {
                    args->nb_packets_to_send = nb;
                }
                break;
            }
            case 'i': {
                int id = atoi(optarg);
                if (id == 0) {
                    perror("Cannot parse bift id");
                } else {
                    args->bift_id = id;
                }
                break;
            }
            case '?': {
                fprintf(stderr, "Message: TODO");
                exit(EXIT_SUCCESS);
            }
            default: {
                fprintf(stderr, "Not supported argument: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
        }
    }
}

int receive_mc_join(uint8_t *packet, ssize_t packet_length, uint64_t *bitstring, int *nb_receivers_ptr) {
    // Packet is: IPv6 + UDP
    // We know the bitstring length because we have a bitstring to set here
    // We do not care about the source of the packet
    uint32_t *payload = (uint32_t *)&packet[sizeof(struct ip6_hdr) + sizeof(struct udphdr)];
    uint32_t join = payload[0]; // 1 if it is a join, 0 otherwise
    uint32_t bfr_id = payload[1];
    if (join) {
        *bitstring |= 1UL << (bfr_id - 1);
        ++(*nb_receivers_ptr);
    } else {
        *bitstring &= ~(1UL << (bfr_id - 1));
        --(*nb_receivers_ptr);
    }

    if (verbose) {
        fprintf(stderr, "   Does it join: %u. The BFER-ID: %u\n", join, bfr_id);
        fprintf(stderr, "   The bitstring is now: %lu\n", bitstring[0]);
    }
}

int main(int argc, char *argv[]) {
    args_t args;
    parse_args(&args, argc, argv);
    verbose = args.verbose;

    // We limit the bitstring to 64 bits
    // Initially, nobody is interested in the multicast flow
    uint64_t bitstring = 0;

    // Socket for communication with the BIER daemon
    int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_un to_bier = {
        .sun_family = AF_UNIX,
    };
    strcpy(to_bier.sun_path, args.bier_daemon_path);

    // The sender uses this path to receive packets from BIER
    struct sockaddr_un from_bier = {
        .sun_family = AF_UNIX,
    };
    strcpy(from_bier.sun_path, args.sender_path);

    // Remove the path if it exists
    // https://medium.com/swlh/getting-started-with-unix-domain-sockets-4472c0db4eb1
    if (remove(from_bier.sun_path) == -1 && errno != ENOENT) {
        perror("removing path");
        goto error1;
    }

    if (bind(socket_fd, (struct sockaddr *)&from_bier, sizeof(struct sockaddr_un)) == -1) {
        perror("bind unix socket");
        goto error1;
    }
    if (verbose) {
        fprintf(stderr, "Bound to UNIX socket: %s\n", args.sender_path);
    }

    // Tell BIER that we want to receive packets for our address
    bier_bind_t bier_bind = {};
    strcpy(bier_bind.unix_path, args.loopback);
    bier_bind.proto = BIERPROTO_IPV6;
    bier_bind.mc_sockaddr.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, args.loopback, &bier_bind.mc_sockaddr.sin6_addr.s6_addr) == 0) {
        perror("IPv6 multicast source address");
        goto error1;
    }
    if (bind_bier_sender(socket_fd, &to_bier, &bier_bind) < 0) {
        goto error1;
    }
    if (verbose) {
        fprintf(stderr, "Bound address %s to BIER daemon\n", args.loopback);
    }

    // Start "asynchronous" procedure
    // Receives: BFER joining the Multicast group
    // Sends: Multicast data
    int nfds = 1;
    struct pollfd pfds = {};
    pfds.fd = socket_fd;
    pfds.events = POLLIN; // Receive
    pfds.events |= POLLOUT; // Send

    // Sending information
    int nb_packets_left = args.nb_packets_to_send;
    int nb_receivers = 0;
    bier_info_t bier_info_out = {};
    bier_info_out.send_info.bift_id = args.bift_id;
    bier_info_out.send_info.bitstring_length = 8;
    bier_info_out.send_info.bitstring = (uint8_t *)&bitstring; // Aliasing :(
    my_packet_t *my_packet = dummy_packet(args.mc_dst);
    if (!my_packet) {
        goto error1;
    }

    // Receiving information
    uint8_t packet[2000];
    bier_info_t _bier_info_in;
    socklen_t addrlen;
    struct sockaddr_in6 _src_received = {};
    
    // Timeout information
    int timeout_send = 1000; // ms
    struct timeval last_sent;
    gettimeofday(&last_sent, NULL);

    while (nb_packets_left > 0) {
        int polled = poll(&pfds, nfds, timeout_send);
        if (polled == -1) {
            perror("poll");
            goto error2;
        } else if (polled == 0) {
            pfds.events |= POLLOUT;
            continue;
        }

        if (pfds.revents & POLLIN) {
            if (verbose) {
                fprintf(stderr, "Received a join demand\n");
            }

            ssize_t received = recvfrom_bier(socket_fd, packet, sizeof(packet), (struct sockaddr *)&_src_received, &addrlen, &_bier_info_in);
            if (received < 0) {
                goto error2;
            }
            receive_mc_join(packet, received, &bitstring, &nb_receivers);
        } else if (pfds.revents & POLLOUT) {
            if (nb_receivers) {
                if (verbose) {
                    fprintf(stderr, "Send out a packet\n");
                }

                ssize_t nb_sent = sendto_bier(socket_fd, my_packet->packet, my_packet->packet_length, (struct sockaddr *)&to_bier, sizeof(to_bier), 6, &bier_info_out);
                if (nb_sent < 0) {
                    goto error2;
                }
            }
            
            // Emulates the timeout, even if no receiver
            pfds.events &= ~(POLLOUT);

        } else {
            fprintf(stderr, "Poll did not work as expected");
            goto error2;
        }

        // Re-activate the sending after some time
        struct timeval now;
        gettimeofday(&now, NULL);
        if (((now.tv_sec - last_sent.tv_sec) * 1000000 + now.tv_usec - last_sent.tv_usec) >= timeout_send * 1000) {
            pfds.events |= POLLOUT;
        }
    }

    // Close and quit
    close(socket_fd);
    free(my_packet);
    exit(EXIT_SUCCESS);

error2:
    free(my_packet);
error1:
    close(socket_fd);
    exit(EXIT_FAILURE);
}