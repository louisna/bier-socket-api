#include "../include/public/multicast.h"

my_packet_t *create_ipv6_from_payload(struct sockaddr_in6 *mc_src,
                                      struct sockaddr_in6 *mc_dst,
                                      const uint32_t payload_length,
                                      const uint8_t *payload) {
    const uint32_t ipv6_header_length = 40;
    const uint32_t udp_header_length = 8;
    const uint32_t packet_total_length =
        ipv6_header_length + udp_header_length + payload_length;
    uint8_t *packet = (uint8_t *)malloc(sizeof(uint8_t) * packet_total_length);
    if (!packet) {
        perror("packet");
        return NULL;
    }
    memset(packet, 0, sizeof(uint8_t) * packet_total_length);
    // Encapsulated IPv6 Header
    struct ip6_hdr *ipv6_header = (struct ip6_hdr *)packet;
    ipv6_header->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);
    ipv6_header->ip6_nxt = 17;  // Nxt hdr = UDP
    ipv6_header->ip6_hops = 44;
    ipv6_header->ip6_plen = htons(8 + payload_length);  // Changed later

    memcpy(&(ipv6_header->ip6_src), &mc_src->sin6_addr, 16);
    memcpy(&(ipv6_header->ip6_dst), &mc_dst->sin6_addr, 16);

    // UDP Header
    struct udphdr *udp_header = (struct udphdr *)&packet[ipv6_header_length];
    udp_header->uh_dport = htons(53982);
    udp_header->uh_sport = htons(53983);
    udp_header->uh_ulen = htons(udp_header_length + payload_length);

    // Payload
    uint8_t *packet_payload = &packet[ipv6_header_length + udp_header_length];
    memcpy(packet_payload, payload, sizeof(uint8_t) * payload_length);

    // Compute UDP checksum
    udp_header->uh_sum =
        udp_checksum(udp_header, sizeof(struct udphdr) + payload_length,
                     &ipv6_header->ip6_src, &ipv6_header->ip6_dst);

    my_packet_t *my_packet = (my_packet_t *)malloc(sizeof(my_packet_t));
    my_packet->packet = packet;
    my_packet->packet_length = packet_total_length;
    if (!my_packet) {
        return NULL;
    }

    return my_packet;
}