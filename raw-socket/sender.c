#include <sys/un.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "include/qcbor-encoding.h"
#include "include/bier-sender.h"

void free_bier(bier_payload_t *bier_payload) {
    free(bier_payload->bitstring);
    free(bier_payload->payload);
    free(bier_payload);
}

bier_payload_t *dummy_packet() {
    bier_payload_t *bier = (bier_payload_t *)malloc(sizeof(bier_payload_t));
    if (!bier) {
        perror("malloc bier");
        return NULL;
    }
    memset(bier, 0, sizeof(bier_payload_t));

    int bitstring_length = 8; // In bytes
    bier->bitstring = (uint8_t *)malloc(sizeof(uint8_t) * bitstring_length);
    if (!bier->bitstring) {
        perror("malloc bitstring");
        free(bier);
        return NULL;
    }
    memset(bier->bitstring, 0, sizeof(uint8_t) * bitstring_length);
    bier->bitstring_length = bitstring_length;
    bier->bitstring[0] = 0b11;

    // Destination of the multicast packet embedded in the BIER packet
    // This must be a multicast address
    char *destination_address = "ff0:babe:cafe::1";
    struct sockaddr_in6 mc_dst = {};

    int err = inet_pton(AF_INET6, destination_address, &mc_dst.sin6_addr.s6_addr);
    if (err == 0)
    {
        perror("IPv6 destination");
        exit(EXIT_FAILURE);
    }

    // Destination of the multicast packet embedded in the BIER packet
    // This must be a multicast address
    char *source_address = "babe::1";
    struct sockaddr_in6 mc_src = {};

    err = inet_pton(AF_INET6, source_address, &mc_src.sin6_addr.s6_addr);
    if (err == 0)
    {
        perror("IPv6 destination");
        exit(EXIT_FAILURE);
    }

    uint8_t payload[1000];
    memset(payload, 0, sizeof(payload));
    payload[999] = 1;
    my_packet_t *packet = create_ipv6_from_payload(&mc_src, &mc_dst, sizeof(payload), payload);
    bier->payload = packet->packet;
    bier->payload_length = packet->packet_length;
    bier->use_bier_te = 1;
    
    // Do not free the payload, it is passed by reference
    free(packet);

    return bier;
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s <UNIX socket path> <nb packets to send> <bitstring> <bifr-id>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }
    int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd == -1)
    {
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
    if (bitstring_arg == 0)
    {
        fprintf(stderr, "Cannot convert forwarding bitmask or no receiver is marked! Given: %s\n", argv[3]);
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

    struct sockaddr_un dst = {}; // Destination is UNIX socket running the BIER process
    dst.sun_family = AF_UNIX;
    strcpy(dst.sun_path, argv[1]);
    int data_len = strlen(dst.sun_path) + sizeof(dst.sun_family);

    bier_payload_t *bier = dummy_packet();
    if (!bier) {
        exit(EXIT_FAILURE);
    }
    printf("First byte of payload: %x (%lu)\n", bier->payload[0], bier->payload_length);

    free(bier->bitstring);
    bier->bitstring = (uint8_t *)&bitstring_arg;
    bier->use_bier_te = bift_id; // TODO: change the name "use bier te"

    UsefulBuf_MAKE_STACK_UB(Buffer, bier->bitstring_length + bier->payload_length + sizeof(bier->bitstring_length) + sizeof(bier->payload_length) + sizeof(bier->use_bier_te) + 200);
    UsefulBufC EncodedEngine = encode_bier_payload(Buffer, bier);
    if (UsefulBuf_IsNULLC(EncodedEngine)) {
        perror("qcbor");
        free_bier(bier);
        exit(EXIT_FAILURE);
    }

    double interval = 500; // ms

    for (int i = 0; i < nb_packets_to_send; ++i) {
        if (sendto(socket_fd, EncodedEngine.ptr, EncodedEngine.len, 0, (struct sockaddr *)&dst, sizeof(struct sockaddr_un)) == -1)
        {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
        sleep((double)interval / 1000.0);
    }


    // free_bier(bier);
    free(bier->payload);
    free(bier);
    close(socket_fd);
    exit(EXIT_SUCCESS);

}