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
#include "bier.h"

typedef struct
{
    struct in6_addr src;
    time_t current;
} data_t;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [host]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct ifaddrs *ifap;
    if (getifaddrs(&ifap) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    char rid[INET6_ADDRSTRLEN];
    struct ifaddrs *iface = NULL;
    struct sockaddr_in6 src;

    for (iface = ifap; iface != NULL; iface = iface->ifa_next)
    {
        if (iface->ifa_addr == NULL)
        {
            continue;
        }

        if (strncmp("lo", iface->ifa_name, 2) == 0 && iface->ifa_addr->sa_family == AF_INET6)
        {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)iface->ifa_addr;
            if (inet_ntop(AF_INET6, addr, rid, INET6_ADDRSTRLEN) == NULL)
            {
                perror("inet_ntop");
                exit(EXIT_FAILURE);
            }

            if (strncmp("::1", rid, 3) != 0)
            {
                fprintf(stderr, "%s %s\n", iface->ifa_name, rid);
                memcpy(&src, addr, sizeof(struct sockaddr_in6));
                break;
            }
        }
    }
    freeifaddrs(ifap);

    data_t data;
    int err = inet_pton(AF_INET6, "::1", &src.sin6_addr.s6_addr);
    if (err == 0)
    {
        fprintf(stderr, "Not in representation format\n");
        exit(EXIT_FAILURE);
    }
    else if (err < 0)
    {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    int socket_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW);
    if (socket_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(struct sockaddr_in6));
    dst.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, argv[1], dst.sin6_addr.s6_addr) != 1)
    {
        perror("inet_ntop dst");
        exit(EXIT_FAILURE);
    }

    uint8_t buff[40 + 20 + 40 + 8 + 10];
    memset(buff, 0, sizeof(buff));

    // IPv6 header
    struct ip6_hdr *iphdr;
    iphdr = (struct ip6_hdr *)&buff[0];
    iphdr->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);
    iphdr->ip6_nxt  = 253; // Nxt hdr = BIER
    iphdr->ip6_hops = 44;
    iphdr->ip6_plen = htons(20 + 40 + 8 + 10); // Changed later

    //IPv6 Source address
    bcopy(&src.sin6_addr, &(iphdr->ip6_src), 16);

	// IPv6 Destination address
	bcopy(&dst.sin6_addr, &(iphdr->ip6_dst), 16);

    uint32_t *bier_header = (uint32_t *)&buff[40];
    memset(bier_header, 1, sizeof(bier_header_t));
    set_bier_proto(bier_header, 6);
    set_bier_bsl(bier_header, 1);
    set_bitstring(bier_header, 0, 0xffffffff);
    set_bitstring(bier_header, 1, 0xffffffff);

    struct ip6_hdr *iphdr_in;
    iphdr_in = (struct ip6_hdr *)&buff[60];
    iphdr_in->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);
    iphdr_in->ip6_nxt  = 17; // Nxt hdr = UDP
    iphdr_in->ip6_hops = 44;
    iphdr_in->ip6_plen = htons(8 + 10); // Changed later

    //IPv6 Source address
    bcopy(&src.sin6_addr, &(iphdr_in->ip6_src), 16);

	// IPv6 Destination address
	bcopy(&dst.sin6_addr, &(iphdr_in->ip6_dst), 16);

    struct udphdr *udp = (struct udphdr *)&buff[100];
    udp->uh_dport = 1234;
    udp->uh_sport = 5678;
    udp->uh_ulen = htons(18);

    for (int i = 0; i < 10; ++i)
    {
        int err = sendto(socket_fd, buff, sizeof(buff), 0, (struct sockaddr *)&dst, sizeof(dst));
        if (err < 0)
        {
            perror("sendto");
        }
    }
}