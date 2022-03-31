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
#include <netinet/udp.h>
#include <math.h>

typedef struct
{
    uint8_t *packet;
    uint32_t packet_length;
} my_packet_t;

void my_packet_free(my_packet_t *pkt);
my_packet_t *encap_bier_packet(uint64_t *bitstring, const uint32_t bitstring_length, uint8_t bier_proto, const uint32_t payload_length, uint8_t *payload);
my_packet_t *create_bier_ipv6_from_payload(uint64_t *bitstring, const uint16_t bitstring_length, struct sockaddr_in6 *mc_src, struct sockaddr_in6 *mc_dst, const uint32_t payload_length, uint8_t *payload);

#endif // BIER_SENDER_H