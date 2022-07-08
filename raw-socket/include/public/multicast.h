#ifndef __BIER_MULTICAST_H__
#define __BIER_MULTICAST_H__

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <netinet/ip6.h>
#include "udp-checksum.h"

/**
 * @brief Personal representation of a packet
 */
typedef struct
{
    uint8_t *packet;
    size_t packet_length;
} my_packet_t;

my_packet_t *create_ipv6_from_payload(struct sockaddr_in6 *mc_src, struct sockaddr_in6 *mc_dst, const uint32_t payload_length, const uint8_t *payload);

#endif