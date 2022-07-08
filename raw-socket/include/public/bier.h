#ifndef __PUBLIC_BIER_H__
#define __PUBLIC_BIER_H__

#include <stdint.h>
#include <netinet/ip6.h>
#include <errno.h>
#include <stdio.h>
#include "qcbor/qcbor.h"
#include "qcbor/qcbor_encode.h"
#include "qcbor/qcbor_decode.h"
#include "qcbor/qcbor_spiffy_decode.h"

typedef struct {
    uint64_t bift_id; // Inserted in the BIER header of the packet
    size_t bitstring_length; // Length of the bitstring in bytes
    uint8_t *bitstring; // Bitstring inserted in the BIER header
} bier_info_t;

/**
 * @brief sendto() like function with a new argument for BIER information instead of a setsockopt()
 * 
 * @param socket UNIX socket linked to the BIER daemon *towards* the BIER daemon
 * @param buf payload of the BIER packet
 * @param len length of `buf` in bytes
 * @param bier_info Information inserted in the BIER header
 * @return ssize_t Number of bytes sent on the socket `socket`
 */
ssize_t sendto_bier(int socket, const void *buf, size_t len, const struct sockaddr *dest_addr, socklen_t addrlen, bier_info_t *bier_info);

/**
 * @brief recvfrom() like function using the BIER mechanism
 * 
 * @param socket UNIX socket linked to the BIER daemon *towards* the application using the BIER daemon
 * @param buf Buffer that will contain the BIER payload packet (without the BIER header)
 * @param len Length of the buffer
 * @param src_addr Address of the upstream BIER Forwarding Router sending the BIER packet inside the BIERin6 tunnel
 * @param addrlen Length of `src_addr`
 * @return ssize_t Number of bytes read from the UNIX socket
 */
ssize_t recvfrom_bier(int socket, void *buf, size_t len, struct sockaddr *src_addr, socklen_t *addrlen);

#endif