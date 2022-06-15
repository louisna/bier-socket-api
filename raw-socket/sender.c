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

bool cmp_bier_payload(bier_payload_t *b1, bier_payload_t *b2) {
    if (b1->payload_length != b2->payload_length || b1->bitstring_length != b2->bitstring_length) {
        return false;
    }
    for (int i = 0; i < b1->bitstring_length; ++i) {
        if (b1->bitstring[i] != b2->bitstring[i]) {
            return false;
        }
    }
    for (int i = 0; i < b1->payload_length; ++i) {
        if (b1->payload[i] != b2->payload[i]) {
            return false;
        }
    }
    return true;
}

int main(int argc, char *argv[])
{
    /*if (argc < 2)
    {
        
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

    char *data = "Hello, UNIX!";

    if (sendto(socket_fd, data, sizeof(char) * (strlen(data) + 1), 0, (struct sockaddr *)&dst, sizeof(struct sockaddr_un)) == -1)
    {
        perror("sendto");
        exit(EXIT_FAILURE);
    }



    close(socket_fd);
    exit(EXIT_SUCCESS);*/

    UsefulBuf_MAKE_STACK_UB(Buffer, 300);

    UsefulBufC EncodedEngine;
    bier_payload_t *bier = (bier_payload_t *)malloc(sizeof(bier_payload_t));
    uint8_t *bitstring = (uint8_t *)malloc(sizeof(uint8_t) * 20);
    uint8_t *payload = (uint8_t *)malloc(sizeof(uint8_t) * 100);
    memset(bitstring, 5, sizeof(uint8_t) * 20);
    memset(payload, 2, sizeof(uint8_t) * 100);

    bier->use_bier_te = 0;
    bier->payload_length = 100;
    bier->bitstring_length = 20;
    bier->payload = payload;
    bier->bitstring = bitstring;

    EncodedEngine = encode_bier_payload(Buffer, bier);
    if (UsefulBuf_IsNULLC(EncodedEngine)) {
        fprintf(stderr, "Cannot encode\n");
        exit(EXIT_FAILURE);
    }

    printf("Encoded data: %zu\n", EncodedEngine.len);

    bier_payload_t bier_output;
    memset(&bier_output, 0, sizeof(bier_payload_t));
    QCBORError uErr = decode_bier_payload(EncodedEngine, &bier_output);
    printf("Decoded with success: %u %u %u\n", uErr == QCBOR_SUCCESS, uErr == QCBOR_ERR_HIT_END, uErr);
    printf("Bier output %lu %lu\n", bier_output.bitstring_length, bier_output.payload_length);

    printf("Recovered data: %u\n", cmp_bier_payload(bier, &bier_output));
    free_bier(bier);

}