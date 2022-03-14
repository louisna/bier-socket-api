#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include "include/bier.h"

typedef struct
{
    uint8_t *packet;
    uint32_t packet_length;
} my_packet_t;

void my_packet_free(my_packet_t *my_packet)
{
    free(my_packet->packet);
    free(my_packet);
}

my_packet_t *create_bier_dummy_packet(uint64_t bitstring, struct sockaddr_in6 *src, struct sockaddr_in6 *dst)
{
    const uint32_t bier_header_length = 12;
    const uint32_t bier_bitstring_length = 64 / 8;
    const uint32_t ipv6_header_length = 40;
    const uint32_t udp_header_length = 8;
    const uint32_t payload_length = 10;
    const uint32_t packet_total_length = bier_header_length + bier_bitstring_length + ipv6_header_length + udp_header_length + payload_length;

    uint8_t *packet = (uint8_t *)malloc(sizeof(uint8_t) * packet_total_length);
    if (!packet)
    {
        perror("packet");
        return NULL;
    }
    memset(packet, 0, sizeof(uint8_t) * packet_total_length);

    my_packet_t *my_packet = (my_packet_t *)malloc(sizeof(my_packet));
    if (!my_packet)
    {
        free(packet);
        perror("my_packet_t");
        return NULL;
    }

    // BIER Header
    uint8_t *bier_header = packet;
    memset(bier_header, 1, sizeof(uint8_t) * bier_header_length);
    bier_header[0] = 0xff;
    set_bier_proto(bier_header, 6);
    set_bier_bsl(bier_header, 1);
    set_bitstring(bier_header, 0, bitstring & 0xff);
    set_bitstring(bier_header, 1, bitstring >> 32);

    // Encapsulated IPv6 Header
    struct ip6_hdr *ipv6_header = (struct ip6_hdr *)&packet[bier_header_length + bier_bitstring_length];
    ipv6_header->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);
    ipv6_header->ip6_nxt = 17; // Nxt hdr = UDP
    ipv6_header->ip6_hops = 44;
    ipv6_header->ip6_plen = htons(8 + 10); // Changed later

    bcopy(&src->sin6_addr, &(ipv6_header->ip6_src), 16); // TODO: replace 16 by addrlen
    bcopy(&dst->sin6_addr, &(ipv6_header->ip6_dst), 16);

    // UDP Header
    struct udphdr *udp_header = (struct udphdr *)&packet[bier_header_length + bier_bitstring_length + ipv6_header_length];
    udp_header->uh_dport = 1234;
    udp_header->uh_sport = 5678;
    udp_header->uh_ulen = htons(udp_header_length + payload_length);

    // Payload
    uint8_t *payload = &packet[bier_header_length + bier_bitstring_length + ipv6_header_length + udp_header_length];
    memset(payload, 0xc, sizeof(uint8_t) * payload_length);

    my_packet->packet = packet;
    my_packet->packet_length = packet_total_length;
    return my_packet;
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s [config_file] [interval - ms] [bitstring]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int interval = atoi(argv[2]);
    if (interval == 0)
    {
        fprintf(stderr, "Cannot convert interval into int: %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];
    bier_internal_t *bier = read_config_file(filename);
    if (!bier)
    {
        exit(EXIT_FAILURE);
    }

    uint64_t bitstring_arg = (uint64_t)strtoull(argv[3], NULL, 16);
    if (bitstring_arg == 0)
    {
        fprintf(stderr, "Cannot convert forwarding bitmask or no receiver is marked! Given: %s\n", argv[3]);
        exit(EXIT_FAILURE);
    }

    int socket_fd = socket(AF_INET6, SOCK_RAW, 253);
    if (socket_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in6 local;
    memset(&local, 0, sizeof(struct sockaddr_in6));
    local.sin6_family = AF_INET6;
    memcpy(&local.sin6_addr.s6_addr, bier->local.s6_addr, sizeof(bier->local.s6_addr));

    int err = bind(socket_fd, (struct sockaddr *)&local, sizeof(local));
    if (err < 0)
    {
        perror("bind local");
        exit(EXIT_FAILURE);
    }

    // Destination of the multicast packet embedded in the BIER packet
    // This must be a multicast address
    char *destination_address = "::2";
    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(struct sockaddr_in6));
    
    err = inet_pton(AF_INET6, destination_address, &dst.sin6_addr.s6_addr);
    if (err == 0)
    {
        perror("IPv6 destination");
        exit(EXIT_FAILURE);
    }
    my_packet_t *my_packet = create_bier_dummy_packet(bitstring_arg, &local, &dst);
    if (!my_packet)
    {
        free_bier_bft(bier);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        fprintf(stderr, "Sending a new packet\n");
        err = bier_processing(my_packet->packet, my_packet->packet_length, socket_fd, bier);
        if (err < 0)
        {
            fprintf(stderr, "Error when processing the BIER packet at the sender... exiting...\n");
            break;
        }
        fprintf(stderr, "Sent a new packet!\n");
        sleep(interval);
    }

    // Free entire system
    my_packet_free(my_packet);
    fprintf(stderr, "Closing the program\n");
    free_bier_bft(bier);
    close(socket_fd);
    exit(EXIT_FAILURE);
}
