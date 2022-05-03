#include "../include/bier.h"
#include "../include/bier-sender.h"

bier_header_t *init_bier_header(const uint64_t *bitstring, const uint32_t bitstring_length, uint8_t bier_proto)
{
    bier_header_t *bh = (bier_header_t *)malloc(sizeof(bier_header_t));
    if (!bh)
    {
        perror("malloc bh");
        return NULL;
    }
    memset(bh, 0, sizeof(bier_header_t));

    const uint32_t bier_header_length = 12;
    const uint32_t bitstring_length_bytes = bitstring_length / 8;
    const uint32_t bier_bsl = log2(bitstring_length) - 5;
    bh->header_length = bier_header_length + bitstring_length_bytes;

    bh->_header = (uint8_t *)malloc(sizeof(uint8_t) * bh->header_length);
    if (!bh->_header)
    {
        perror("bh header");
        free(bh);
        return NULL;
    }

    set_bier_proto(bh->_header, bier_proto);

    for (uint16_t i = 0; i < bitstring_length_bytes / 8; ++i)
    {
        set_bitstring(bh->_header, i, bitstring[i]);
    }
    set_bier_bsl(bh->_header, bier_bsl);

    return bh;
}

void release_bier_header(bier_header_t *bh)
{
    free(bh->_header);
    free(bh);
}

void set_bh_proto(bier_header_t *bh, uint8_t proto)
{
    set_bier_proto(bh->_header, proto);
}

void update_bh_bitstring(bier_header_t *bh, const uint32_t bitstring_length, uint64_t *bitstring)
{
    for (uint16_t i = 0; i < bitstring_length / 8; ++i)
    {
        set_bitstring(bh->_header, i, bitstring[i]);
    }
}

void my_packet_free(my_packet_t *my_packet)
{
    free(my_packet->packet);
    free(my_packet);
}

my_packet_t *encap_bier_packet(bier_header_t *bh, const uint32_t payload_length, uint8_t *payload)
{
    const uint32_t packet_total_length = bh->header_length + payload_length;

    my_packet_t *my_packet = (my_packet_t *)malloc(sizeof(my_packet_t));
    if (!my_packet)
    {
        perror("malloc my_packet");
        return NULL;
    }

    my_packet->packet_length = packet_total_length;
    my_packet->packet = (uint8_t *)malloc(sizeof(uint8_t) * packet_total_length);
    if (!my_packet->packet)
    {
        perror("malloc packet");
        free(my_packet);
        return NULL;
    }
    memset(my_packet->packet, 0, sizeof(uint8_t) * packet_total_length);

    uint8_t *bier_header = my_packet->packet;
    memcpy(bier_header, bh->_header, bh->header_length);

    // Copy the payload of the packet inside the packet buffer
    uint8_t *packet_payload = (uint8_t *)&my_packet->packet[bh->header_length];
    memcpy(packet_payload, payload, sizeof(uint8_t) * payload_length);

    return my_packet;
}

my_packet_t *create_bier_ipv6_from_payload(bier_header_t *bh, struct sockaddr_in6 *mc_src, struct sockaddr_in6 *mc_dst, const uint32_t payload_length, const uint8_t *payload)
{
    fprintf(stderr, "dummy_packet %p %u\t", payload, payload_length);
    for (int i = 0; i < payload_length; i++)
    {
        fprintf(stderr, "%x", *(payload + i));
    }
    fprintf(stderr, "\n");

    const uint32_t ipv6_header_length = 40;
    const uint32_t udp_header_length = 8;
    const uint32_t packet_total_length = ipv6_header_length + udp_header_length + payload_length;
    uint8_t *packet = (uint8_t *)malloc(sizeof(uint8_t) * packet_total_length);
    if (!packet)
    {
        perror("packet");
        return NULL;
    }
    memset(packet, 0, sizeof(uint8_t) * packet_total_length);
    // Encapsulated IPv6 Header
    struct ip6_hdr *ipv6_header = (struct ip6_hdr *)packet;
    ipv6_header->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);
    ipv6_header->ip6_nxt = 17; // Nxt hdr = UDP
    ipv6_header->ip6_hops = 44;
    ipv6_header->ip6_plen = htons(8 + payload_length); // Changed later

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
    udp_header->uh_sum = udp_checksum(udp_header, sizeof(struct udphdr) + payload_length, &ipv6_header->ip6_src, &ipv6_header->ip6_dst);

    my_packet_t *my_packet = encap_bier_packet(bh, packet_total_length, packet);
    if (!my_packet)
    {
        return NULL;
    }

    free(packet);
    return my_packet;
}
