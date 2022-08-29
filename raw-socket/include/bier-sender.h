#ifndef BIER_SENDER_H
#define BIER_SENDER_H

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "bier.h"
#include "udp-checksum.h"

/**
 * @brief Personal representation of a packet
 */
typedef struct {
    uint8_t *packet;
    uint32_t packet_length;
} my_packet_t;

/**
 * @brief Personal representatio of a BIER header. The content of the header
 * should not be accessed directly.
 */
typedef struct {
    uint8_t *_header;
    uint32_t header_length;
} bier_header_t;

typedef union {
    struct in6_addr v6;
    struct in_addr v4;
} in_addr_common_t;

typedef struct {
    int nb_entries;
    in_addr_common_t *addrs;
    uint64_t *bfr_ids;
} bier_addr2bifr_t;

typedef struct {
    int nb_entries;
    struct mc_entry {
        int bifr_id;
        int family; // AF_INET or AF_INET6
        union {
            struct in6_addr mc_addr6;
            struct in_addr mc_addr4;
        } mc_addr;
        union {
            struct in6_addr src_addr6;
            struct in_addr src_addr4;
        } src_addr;
    } *entries;
} mc_mapping_t;

/**
 * @brief Creates a BIER header with every field set to 0 except the bitstring,
 * the BSL and the proto fields
 *
 * @param bitstring the bitstring of the packet, as an array of size
 * *bitstring_length* of uint64_t values
 * @param bitstring_length the length of the *bitstring* array, in uint64_t unit
 * @param bier_proto the value of the "proto" field of the BIER header (i.e.,
 * the next header)
 * @return bier_header_t* structure containing the ->_packet of ->header_length
 * bytes (including the bitstring)
 */
bier_header_t *init_bier_header(const uint64_t *bitstring,
                                const uint32_t bitstring_length,
                                uint8_t bier_proto, int bift_id);

/**
 * @brief Release the memory associated with the BIER header
 *
 * @param bh pointer to the BIER header structure
 */
void release_bier_header(bier_header_t *bh);

/**
 * @brief Set the bh proto field of the BIER header
 *
 * @param bh BIER header pointer
 * @param proto proto value
 * @return ** void /
 */
void set_bh_proto(bier_header_t *bh, uint8_t proto);

/**
 * @brief Update the bitstring value of the BIER header. /!\ Cannot change the
 * bitstring length, only the values
 *
 * @param bh BIER header pointer
 * @param bitstring_length the current length of the bitstring of the BIER
 * header
 * @param bitstring array of uint64_t values containing the bitstring, array of
 * length *bitstring_length* uint64_t elements
 */
void update_bh_bitstring(bier_header_t *bh, const uint32_t bitstring_length,
                         uint64_t *bitstring);

/**
 * @brief Release the memory associated with the my_packet_t structure
 *
 * @param my_packet pointer to the custom packet
 */
void my_packet_free(my_packet_t *pkt);

/**
 * @brief Encapsulate the payload (may be a packet) given by *payload* of length
 * *payload_length* in a BIER header given by *bh*
 *
 * @param bh the BIER header structure
 * @param payload_length the length of the payload to encapsulate
 * @param payload the payload
 * @return my_packet_t* a new custom packet encapsulated in a BIER header
 */
my_packet_t *encap_bier_packet(bier_header_t *bh, const uint32_t payload_length,
                               uint8_t *payload);

/**
 * @brief Create a dummy packet from an application payload. The payload is
 * encapsulated in a UDP header within an IPv6 header, and finally in a BIER
 * header given by *bh*. The IPv6 source/destination addresses are given by
 * *mc_src* and *mc_dst* respectively
 *
 * @param bh pointer to the BIER header structure
 * @param mc_src IPv6 multicast source of the encapsulated IPv6 header
 * @param mc_dst IPv6 multicast destination of the encapsulared IPv6 header
 * @param payload_length length of the application payload to encapsulate
 * @param payload application payload
 * @return my_packet_t* pointer to the custom packet
 */
my_packet_t *create_bier_ipv6_from_payload(bier_header_t *bh,
                                           struct in6_addr *mc_src,
                                           struct in6_addr *mc_dst,
                                           const uint32_t payload_length,
                                           const uint8_t *payload);

#endif  // BIER_SENDER_H
