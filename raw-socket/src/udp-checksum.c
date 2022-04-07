#include "../include/udp-checksum.h"

uint16_t udp_checksum(const void *buff, size_t len, struct in6_addr *src_addr, struct in6_addr *dest_addr)
{
    const uint16_t *buf = buff;
    uint16_t *ip_src = (void *)src_addr, *ip_dst = (void *)dest_addr;
    uint32_t sum;
    size_t length = len;
    int i;

    /* Calculate the sum */
    sum = 0;
    while (len > 1)
    {
        sum += *buf++;
        if (sum & 0x80000000)
            sum = (sum & 0xFFFF) + (sum >> 16);
        len -= 2;
    }
    if (len & 1)
        /* Add the padding if the packet length is odd */
        sum += *((uint8_t *)buf);

    /* Add the pseudo-header */
    for (i = 0; i <= 7; ++i)
        sum += *(ip_src++);

    for (i = 0; i <= 7; ++i)
        sum += *(ip_dst++);

    sum += htons(IPPROTO_UDP);
    sum += htons(length);

    /* Add the carries */
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    /* Return the one's complement of sum */
    return ((uint16_t)(~sum));
}