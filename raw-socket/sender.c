#include <sys/un.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "include/qcbor-encoding.h"

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
    memset(bier->bitstring, 1, sizeof(uint8_t) * bitstring_length);
    bier->bitstring_length = bitstring_length;

    int payload_length = 1000;
    bier->payload = (uint8_t *)malloc(sizeof(uint8_t) * payload_length);
    if (!bier->payload) {
        perror("malloc payload");
        free(bier->bitstring);
        free(bier);
        return NULL;
    }
    memset(bier->payload, 0xff, sizeof(uint8_t) * payload_length);
    bier->payload_length = payload_length;

    bier->use_bier_te = 1;

    return bier;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        // TODO
    }
    int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un dst = {};
    dst.sun_family = AF_UNIX;
    strcpy(dst.sun_path, "/tmp/socket-bfr");
    int data_len = strlen(dst.sun_path) + sizeof(dst.sun_family);

    bier_payload_t *bier = dummy_packet();
    if (!bier) {
        exit(EXIT_FAILURE);
    }

    UsefulBuf_MAKE_STACK_UB(Buffer, bier->bitstring_length + bier->payload_length + sizeof(bier->bitstring_length) + sizeof(bier->payload_length) + sizeof(bier->use_bier_te) + 200);
    UsefulBufC EncodedEngine = encode_bier_payload(Buffer, bier);
    if (UsefulBuf_IsNULLC(EncodedEngine)) {
        perror("qcbor");
        free_bier(bier);
        exit(EXIT_FAILURE);
    }

    if (sendto(socket_fd, EncodedEngine.ptr, EncodedEngine.len, 0, (struct sockaddr *)&dst, sizeof(struct sockaddr_un)) == -1)
    {
        perror("sendto");
        exit(EXIT_FAILURE);
    }



    close(socket_fd);
    exit(EXIT_SUCCESS);

}