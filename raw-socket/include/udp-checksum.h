#ifndef UDP_CHECKSUM_H
#define UDP_CHECKSUM_H

#include <stdint.h>
#include <netinet/udp.h>
#include <netinet/in.h>

/**
 * @brief Computes the UDP checksum based on an IPv6 pseudo-header.
 * From https://github.com/gih900/IPv6--DNS-Frag-Test-Rig/blob/master/dns-server-frag.c
 * 
 * @param buff the content of the packet starting at the UDP header
 * @param len the length of `buff` on bytes
 * @param src_addr the IPv6 source address
 * @param dest_addr the IPv6 destination address
 * @return uint16_t checksum of the IPv6-UDP pseudo header
 */
uint16_t udp_checksum(const void *buff, size_t len, struct in6_addr *src_addr, struct in6_addr *dest_addr);

#endif // UDP_CHECKSUM_H