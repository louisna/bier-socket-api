#ifndef UDP_CHECKSUM_H
#define UDP_CHECKSUM_H

#include <stdint.h>
#include <netinet/udp.h>
#include <netinet/in.h>

/* https://github.com/gih900/IPv6--DNS-Frag-Test-Rig/blob/master/dns-server-frag.c */
uint16_t udp_checksum(const void *buff, size_t len, struct in6_addr *src_addr, struct in6_addr *dest_addr);

#endif // UDP_CHECKSUM_H