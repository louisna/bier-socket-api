#ifndef LOCAL_PROCESSING_H
#define LOCAL_PROCESSING_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include "bier.h"
#include "udp-checksum.h"

typedef struct
{
    int raw_socket;
    struct sockaddr_in6 dst;
    struct in6_addr src;
} raw_socket_arg_t;

/**
 * @brief This is an example of a function that handles a BIER packet whose destination is the local router processing it.
 * It just transmits the content of the BIER packet to the socket given in argument
 *
 * @param bier_packet BIER packet buffer. The BIER header is still in the buffer in case the application needs to retrieve information from it
 * @param packet_length the length of the packet buffer
 * @param bier_header_length the length of the BIER header (at the beginning of the packet)
 * @param args additional arguments given to the function. The function is responsible to know how to parse these arguments
 */
void local_behaviour(const uint8_t *bier_packet, const uint32_t packet_length, const uint32_t bier_header_length, void *args);

#endif // LOCAL_PROCESSING_H