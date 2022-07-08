#ifndef __BIER_MULTICAST_H__
#define __BIER_MULTICAST_H__

#include <netinet/ip6.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "udp-checksum.h"

/**
 * @brief Personal representation of a packet
 */
typedef struct {
    uint8_t *packet;
    size_t packet_length;
} my_packet_t;

/**
 * @brief Create an IPv6 packet with a UDP transport header from a defined
 * payload
 *
 * @param mc_src Multicast source socket address
 * @param mc_dst Multicast destination socket address
 * @param payload_length Length of the payload
 * @param payload Payload encapsulated in the IPv6 and UDP header
 * @return my_packet_t*
 */
my_packet_t *create_ipv6_from_payload(struct sockaddr_in6 *mc_src,
                                      struct sockaddr_in6 *mc_dst,
                                      const uint32_t payload_length,
                                      const uint8_t *payload);

#endif