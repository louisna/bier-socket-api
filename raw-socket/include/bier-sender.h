#ifndef BIER_SENDER_H
#define BIER_SENDER_H

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <math.h>
#include "bier.h"

typedef struct
{
    uint8_t *packet;
    uint32_t packet_length;
} my_packet_t;

typedef struct
{
    uint8_t *_header;
    uint32_t header_length;
} bier_header_t;

bier_header_t *init_bier_header(uint64_t *bitstring, const uint32_t bitstring_length, uint8_t bier_proto);
void release_bier_header(bier_header_t *bh);
void set_bh_proto(bier_header_t *bh, uint8_t proto);
void update_bh_bitstring(bier_header_t *bh, const uint32_t bitstring_length, uint64_t *bitstring);

void my_packet_free(my_packet_t *pkt);
my_packet_t *encap_bier_packet(bier_header_t *bh, const uint32_t payload_length, uint8_t *payload);
my_packet_t *create_bier_ipv6_from_payload(bier_header_t *bh, struct sockaddr_in6 *mc_src, struct sockaddr_in6 *mc_dst, const uint32_t payload_length, const uint8_t *payload);
int send_payload(bier_internal_t *bier, uint64_t bitstring, const void* payload, size_t payload_length);

#endif // BIER_SENDER_H
