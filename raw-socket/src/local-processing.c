#include "../include/local-processing.h"

void local_behaviour(const uint8_t *bier_packet, const uint32_t packet_length, const uint32_t bier_header_length, void *args)
{
    raw_socket_arg_t *raw_args = (raw_socket_arg_t *)args;
    size_t v6_packet_length = packet_length - bier_header_length;
    uint8_t ipv6_packet[v6_packet_length];
    size_t payload_length = v6_packet_length - sizeof(struct ip6_hdr) - sizeof(struct udphdr);
    memcpy((void *)&ipv6_packet, &bier_packet[bier_header_length], v6_packet_length);
    struct udphdr *udp = (struct udphdr *)&ipv6_packet[sizeof(struct ip6_hdr)];
    uint8_t *payload = &ipv6_packet[sizeof(struct ip6_hdr) + sizeof(struct udphdr)];

    for (int i = 0; i < payload_length; i++)
        fprintf(stderr, "%x", payload[i]);
    fprintf(stderr, "\n");

    fprintf(stderr, "before local sendto\n");
    for (int i = 0; i < v6_packet_length; i++)
        fprintf(stderr, "%x", ipv6_packet[i]);
    fprintf(stderr, "\n");

    struct ip6_hdr *hdr = (struct ip6_hdr *)&ipv6_packet[0];
    memcpy(&(hdr->ip6_dst), &raw_args->dst.sin6_addr, sizeof(raw_args->dst.sin6_addr));
    memcpy(&(hdr->ip6_src), &raw_args->src, sizeof(raw_args->src));
    udp->uh_sum = 0; // Reset for new computation
    uint16_t chksm = udp_checksum(udp, sizeof(struct udphdr) + payload_length, &hdr->ip6_src, &hdr->ip6_dst);
    udp->uh_sum = chksm;

    fprintf(stderr, "local sendto after\n");
    for (int i = 0; i < v6_packet_length; i++)
        fprintf(stderr, "%x", ipv6_packet[i]);
    fprintf(stderr, "\n");
    if (sendto(raw_args->raw_socket, ipv6_packet, v6_packet_length, 0, (struct sockaddr *)&raw_args->dst, sizeof(raw_args->dst)) != v6_packet_length)
    {
        perror("Cannot send using raw socket... ignoring");
    }
}