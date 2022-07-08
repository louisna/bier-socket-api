#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "include/public/bier.h"
#include "include/public/multicast.h"

my_packet_t *dummy_packet() {
    // Destination of the multicast packet embedded in the BIER packet
    // This must be a multicast address
    char *destination_address = "ff0:babe:cafe::1";
    struct sockaddr_in6 mc_dst = {};

    int err =
        inet_pton(AF_INET6, destination_address, &mc_dst.sin6_addr.s6_addr);
    if (err == 0) {
        perror("IPv6 destination");
        exit(EXIT_FAILURE);
    }

    // Destination of the multicast packet embedded in the BIER packet
    // This must be a multicast address
    char *source_address = "babe::1";
    struct sockaddr_in6 mc_src = {};

    err = inet_pton(AF_INET6, source_address, &mc_src.sin6_addr.s6_addr);
    if (err == 0) {
        perror("IPv6 destination");
        exit(EXIT_FAILURE);
    }

    uint8_t payload[1000];
    memset(payload, 0, sizeof(payload));
    payload[999] = 1;
    my_packet_t *packet =
        create_ipv6_from_payload(&mc_src, &mc_dst, sizeof(payload), payload);

    return packet;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr,
                "Usage: %s <UNIX socket path> <nb packets to send> <bitstring> "
                "<bift-id>\n",
                argv[0]);
        exit(EXIT_SUCCESS);
    }
    int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int nb_packets_to_send = atoi(argv[2]);
    if (nb_packets_to_send == 0) {
        fprintf(stderr, "Cannot convert to integer: %s\n", argv[2]);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    uint64_t bitstring_arg = (uint64_t)strtoull(argv[3], NULL, 16);
    if (bitstring_arg == 0) {
        fprintf(stderr,
                "Cannot convert forwarding bitmask or no receiver is marked! "
                "Given: %s\n",
                argv[3]);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }
    printf("Bitstring: %lx\n", bitstring_arg);

    int bift_id = atoi(argv[4]);
    if (bift_id == 0) {
        fprintf(stderr, "Cannot convert bifr id: %s\n", argv[4]);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un dst =
        {};  // Destination is UNIX socket running the BIER process
    dst.sun_family = AF_UNIX;
    strcpy(dst.sun_path, argv[1]);
    int data_len = strlen(dst.sun_path) + sizeof(dst.sun_family);

    my_packet_t *packet = dummy_packet();
    if (!packet) {
        exit(EXIT_FAILURE);
    }
    printf("First byte of payload: %x (%lu bytes long)\n", packet->packet[0],
           packet->packet_length);

    bier_info_t bier_info = {};
    bier_info.bitstring = (uint8_t *)&bitstring_arg;
    bier_info.bitstring_length = 8;
    bier_info.bift_id = bift_id;
    fprintf(stderr, "The BIFT-ID is %lu\n", bier_info.bift_id);

    double interval = 500;  // ms
    for (int i = 0; i < nb_packets_to_send; ++i) {
        fprintf(stderr, "The first few bytes are: ");
        for (int j = 0; j < 10; ++j) {
            fprintf(stderr, "%x ", packet->packet[j]);
        }
        fprintf(stderr, "\n");
        size_t nb_sent =
            sendto_bier(socket_fd, packet->packet, packet->packet_length,
                        (struct sockaddr *)&dst, sizeof(dst), &bier_info);
        if (nb_sent < 0) {
            perror("Sender sendto_bier");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "Sent %lu bytes\n", nb_sent);
        sleep(1);
    }

    close(socket_fd);
    exit(EXIT_SUCCESS);
}