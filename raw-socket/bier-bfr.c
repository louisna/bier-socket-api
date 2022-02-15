#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>

#include "bier.h"

typedef struct
{
    uint32_t bfr_id;
    uint32_t forwarding_bitmask;
    int32_t bfr_nei; // BIER Forwarding Router Neighbour
    struct sockaddr_in6 bfr_nei_addr;
} bier_bft_entry_t;

typedef struct
{
    struct in6_addr local;
    bier_bft_entry_t *bft;
    int socket;
} bier_internal_t;

void print_buffer(uint8_t *buffer, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        if (buffer[i] < 16)
        {
            printf("0");
        }
        printf("%x ", buffer[i]);
    }
    printf("\n");
}

int encapsulate_ipv6(uint8_t *buffer, uint32_t buffer_length)
{
    struct ip6_hdr *iphdr;
    iphdr = (struct ip6_hdr *)buffer;
    iphdr->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);
    iphdr->ip6_nxt  = 253; // Nxt hdr = UDP
    iphdr->ip6_hops = 11;
    printf("Packet length is: %u %d", buffer_length, buffer_length);
    iphdr->ip6_plen = htons((uint16_t)buffer_length); // Changed later

    //IPv6 Source address
    struct sockaddr_in6 src;
    memset(&src, 0, sizeof(struct sockaddr_in6));
    src.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, "::3", src.sin6_addr.s6_addr) != 1)
    {
        perror("inet_ntop src");
        return -1;
    }
    bcopy(&src.sin6_addr, &(iphdr->ip6_src), 16);

	// IPv6 Destination address
    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(struct sockaddr_in6));
    dst.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, "::1", dst.sin6_addr.s6_addr) != 1)
    {
        perror("inet_ntop dst");
        return -1;
    }
	bcopy(&dst.sin6_addr, &(iphdr->ip6_dst), 16);

    return 0;
}

int bier_processing(uint8_t *buffer, size_t buffer_length, int socket_fd)
{
    // As specified in RFC 8296, we cannot rely on the `bsl` field of the BIER header
    // but we must know with the bift_id the true BSL
    uint32_t bift_id = get_bift_id(buffer);
    if (bift_id != 1 && 0)
    {
        fprintf(stderr, "Wrong BIFT_ID: must drop the packet: %u", bift_id);
        return -1;
    }

    // Here we can assume that we know the BSL: 0 meaning that the bitstring has a length of 4 bytes
    uint32_t bitstring_length = 4; // Bytes
    uint32_t bitstring_idx = 0; // Offset in the bitstring
    uint32_t bitstring = get_bitstring(buffer, bitstring_idx);

    // RFC 8279
    uint32_t idx_bfr = 0;
    while ((bitstring >> idx_bfr) > 0)
    {
        if ((bitstring >> idx_bfr) & 1) // The current lowest-order bit is set: this BFER must receive a copy
        {
            fprintf(stderr, "Send a copy to %u\n", idx_bfr);
            uint8_t packet_copy[buffer_length]; // Room for the IPv6 header
            memset(packet_copy, 0, sizeof(packet_copy));

            uint8_t *bier_header = &packet_copy[0];
            memcpy(bier_header, buffer, buffer_length);

            uint32_t bitstring_copy = bitstring;
            printf("BitString value is: %x\n", bitstring_copy);
            bitstring_copy &= 0xffff;
            set_bitstring(bier_header, 0, bitstring_copy);
            printf("BitString value is now: %x\n", bitstring_copy);

            // Send copy
            int err = 0; //encapsulate_ipv6(packet_copy, buffer_length);
            if (err < 0)
            {
                return err;
            }

            struct sockaddr_in6 dst;
            memset(&dst, 0, sizeof(struct sockaddr_in6));
            dst.sin6_family = AF_INET6;
            if (inet_pton(AF_INET6, "::1", dst.sin6_addr.s6_addr) != 1)
            {
                perror("inet_ntop dst");
                exit(EXIT_FAILURE);
            }
            err = sendto(socket_fd, packet_copy, sizeof(packet_copy), 0, (struct sockaddr *)&dst, sizeof(dst));
            if (err < 0)
            {
                perror("sendto");
                return -1;
            }
            printf("Sent packet\n");
            bitstring &= ~(0xf | (1 << idx_bfr));
        }
        ++idx_bfr; // Keep track of the index of the BFER to get the correct entry of the BFT
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];

    // Open the file and setup the BIER router

    int socket_fd = socket(AF_INET6, SOCK_RAW, 253);
    if (socket_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in6 local;
    memset(&local, 0, sizeof(struct sockaddr_in6));
    local.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, argv[1], local.sin6_addr.s6_addr) != 1)
    {
        perror("inet_ntop local");
        exit(EXIT_FAILURE);
    }

    int err = bind(socket_fd, (struct sockaddr *)&local, sizeof(local));
    if (err < 0)
    {
        perror("bind local");
        exit(EXIT_FAILURE);
    }

    uint8_t buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    size_t length = recv(socket_fd, buffer, 4096, 0);
    printf("length=%lu\n", length);
    print_buffer(buffer, length);
    bier_processing(buffer, length, socket_fd);
}