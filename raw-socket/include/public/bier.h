#ifndef __PUBLIC_BIER_H__
#define __PUBLIC_BIER_H__

#include <netinet/ip6.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"

typedef struct {
    union {
        struct {
            uint64_t bift_id;  // Inserted in the BIER header of the packet
            size_t bitstring_length;  // Length of the bitstring in bytes
            uint8_t *bitstring;       // Bitstring inserted in the BIER header
        } send_info;
        struct {
            uint64_t upstream_router_bfr_id;
        } recv_info;
    };
} bier_info_t;

/**
 * @brief sendto() like function with a new argument for BIER information
 * instead of a setsockopt()
 *
 * @param socket UNIX socket linked to the BIER daemon *towards* the BIER daemon
 * @param buf payload of the BIER packet
 * @param len length of `buf` in bytes
 * @param proto the protocol number following the BIER header
 * @param bier_info Information inserted in the BIER header
 * @return ssize_t Number of bytes sent on the socket `socket`
 */
ssize_t sendto_bier(int socket, const void *buf, size_t len,
                    const struct sockaddr *dest_addr, socklen_t addrlen,
                    uint16_t proto, bier_info_t *bier_info);

/**
 * @brief recvfrom() like function using the BIER mechanism
 *
 * @param socket UNIX socket linked to the BIER daemon *towards* the application
 * using the BIER daemon
 * @param buf Buffer that will contain the BIER payload packet (without the BIER
 * header)
 * @param len Length of the buffer
 * @param src_addr Address of the upstream BIER Forwarding Router sending the
 * BIER packet inside the BIERin6 tunnel
 * @param addrlen Length of `src_addr`
 * @return ssize_t Number of bytes read from the UNIX socket
 */
ssize_t recvfrom_bier(int socket, void *buf, size_t len,
                      struct sockaddr *src_addr, socklen_t *addrlen,
                      bier_info_t *bier_info);

/**
 * @brief Binds the application to BIER socket with IPV6 Multicast address
 *
 * @param socket UNIX socket used to forward the message to the BIER daemon
 * @param bier_sock_path Path to the UNIX socket of the BIER daemon
 * @param bind_to Contains the binding information
 * @return int 0 if success, -1 otherwise
 */
int bind_bier(int socket, const struct sockaddr_un *bier_sock_path,
              bier_bind_t *bind_to);


int bind_bier_sender(int socket, const struct sockaddr_un *bier_sock_path,
                     bier_bind_t *bind_to);

#endif