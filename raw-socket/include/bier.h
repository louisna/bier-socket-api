#ifndef BIER_H
#define BIER_H

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
#include <netinet/udp.h>

#define get_bift_id(data) (be32toh(data[0]) >> 12)
#define get_bitstring(data, bitstring_idx) (htobe64(((uint64_t *)&((uint32_t *)data)[3 + bitstring_idx])))
#define get_bitstring_ptr(data) ((uint64_t *)(&(data)[12]))
#define set_bitstring(data, bitstring_idx, bitstring) \
    {                                                 \
        uint32_t *d32 = (uint32_t *)data;             \
        uint64_t *d64 = (uint64_t *)&d32[3];          \
        d64[bitstring_idx] = htobe64(bitstring);      \
    }
#define set_bitstring_ptr(data, bitstring_ptr, bitstring_max_idx)         \
    {                                                                     \
        uint32_t *d32 = (uint32_t *)data;                                 \
        uint64_t *d64 = (uint64_t *)&d32[3];                              \
        memcpy(d64, bitstring_ptr, sizeof(uint64_t) * bitstring_max_idx); \
    }
#define set_bier_proto(d, proto)    \
    {                               \
        uint8_t *d8 = (uint8_t *)d; \
        d8[9] &= 0xc0;              \
        d8[9] |= (proto & 0x3f);    \
    }
#define set_bier_bsl(d, bsl)        \
    {                               \
        uint8_t *d8 = (uint8_t *)d; \
        d8[5] &= 0x0f;              \
        d8[5] |= (bsl << 4);        \
    }

typedef enum
{
    bitwise_u64_and_not,
    bitwise_u64_and
} bitstring_operation;

typedef struct
{
    uint32_t bfr_id;
    uint64_t *forwarding_bitmask;
    uint32_t bitstring_length;
    int32_t bfr_nei; // BIER Forwarding Router Neighbour
    struct sockaddr_in6 bfr_nei_addr;
} bier_bft_entry_t;

typedef struct
{
    int local_bfr_id;
    struct in6_addr local;
    int nb_bft_entry;
    uint32_t bitstring_length; // In bits
    int socket;
    bier_bft_entry_t **bft;
} bier_internal_t;

typedef struct
{
    void *args;
    void (*local_processing_function)(const uint8_t *bier_packet, const uint32_t packet_length, const uint32_t bier_header_length, void *args);
} bier_local_processing_t;

// TODO: this structure will go to the application file
typedef struct
{
    int raw_socket;
    struct sockaddr_in6 local;
    struct in6_addr src;
} raw_socket_arg_t;

bier_internal_t *read_config_file(char *config_filepath);
void free_bier_bft(bier_internal_t *bft);
int bier_processing(uint8_t *buffer, size_t buffer_length, int socket_fd, bier_internal_t *bft, bier_local_processing_t *bier_local_processing);
void print_bft(bier_internal_t *bft);
void send_to_raw_socket(const uint8_t *bier_packet, const uint32_t packet_length, const uint32_t bier_header_length, void *args);
uint16_t udp_checksum (const void *buff, size_t len, struct in6_addr *src_addr, struct in6_addr *dest_addr);

#endif // BIER_H
