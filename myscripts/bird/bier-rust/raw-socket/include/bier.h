#include <stdint.h>
#ifndef __APPLE__
#include <endian.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <unistd.h>
#include <signal.h>

#define get_bift_id(data) (be32toh(data[0]) >> 12)
#define get_bitstring(data, bitstring_idx) (htobe32(((uint32_t *)data)[3 + bitstring_idx]))
#define set_bitstring(data, bitstring_idx, bitstring) \
    {                                                 \
        uint32_t *d32 = (uint32_t *)data;             \
        d32[3 + bitstring_idx] = htobe32(bitstring);  \
    }
#define set_bier_proto(d, proto)        \
    {                                   \
        uint8_t *d8 = (uint8_t *)d;     \
        d8[9] &= 0xc0;                  \
        d8[9] |= (proto & 0x3f);        \
    }
#define set_bier_bsl(d, bsl)            \
    {                                   \
        uint8_t *d8 = (uint8_t *)d; \
        d8[5] &= 0x0f;                  \
        d8[5] |= (bsl << 4);            \
    }

typedef struct
{
    uint32_t bfr_id;
    uint32_t forwarding_bitmask;
    int32_t bfr_nei; // BIER Forwarding Router Neighbour
    struct sockaddr_in6 bfr_nei_addr;
} bier_bft_entry_t;

typedef struct
{
    int local_bfr_id;
    struct in6_addr local;
    int nb_bft_entry;
    int socket;
    bier_bft_entry_t **bft;
} bier_internal_t;

bier_internal_t *read_config_file(char *config_filepath);
void free_bier_bft(bier_internal_t *bft);
int bier_processing(uint8_t *buffer, size_t buffer_length, int socket_fd, bier_internal_t *bft);
