#include "include/bier.h"
#include "include/bier-sender.h"

void my_packet_free(my_packet_t *my_packet)
{
    free(my_packet->packet);
    free(my_packet);
}

my_packet_t *encap_bier_packet(uint64_t *bitstring, const uint32_t bitstring_length, uint8_t bier_proto, const uint32_t payload_length, uint8_t *payload)
{
    const uint32_t bier_header_length = 12;
    const uint32_t bitstring_length_bytes = bitstring_length / 8;
    const uint32_t bier_bsl = log2(bitstring_length) - 5;
    const uint32_t packet_total_length = bier_header_length + bitstring_length_bytes + payload_length;

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
    // TODO: remove this memset and replace by true operations
    memset(bier_header, 1, sizeof(uint8_t) * bier_header_length);
    bier_header[0] = 0xff;
    set_bier_proto(bier_header, bier_proto);
    for (uint16_t i = 0; i < bitstring_length_bytes; ++i)
    {
        set_bitstring(bier_header, i, bitstring[i]);
    }
    set_bier_bsl(bier_header, bier_bsl);

    // Copy the payload of the packet inside the packet buffer
    uint8_t *packet_payload = (uint8_t *)&my_packet->packet[bier_header_length + bitstring_length_bytes];
    memcpy(packet_payload, payload, sizeof(uint8_t) * payload_length);

    return my_packet;
}

my_packet_t *create_bier_ipv6_from_payload(uint64_t *bitstring, const uint16_t bitstring_length, struct sockaddr_in6 *mc_src, struct sockaddr_in6 *mc_dst, const uint32_t payload_length, uint8_t *payload)
{
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

    memcpy(&mc_src->sin6_addr, &(ipv6_header->ip6_src), 16);
    memcpy(&mc_dst->sin6_addr, &(ipv6_header->ip6_dst), 16);

    // UDP Header
    struct udphdr *udp_header = (struct udphdr *)&packet[ipv6_header_length];
    udp_header->uh_dport = 1234;
    udp_header->uh_sport = 5678;
    udp_header->uh_ulen = htons(udp_header_length + payload_length);

    // Payload
    uint8_t *packet_payload = &packet[ipv6_header_length + udp_header_length];
    memcpy(packet_payload, payload, sizeof(uint8_t) * payload_length);

    my_packet_t *my_packet = encap_bier_packet(bitstring, bitstring_length, 6, packet_total_length, packet);
    if (!my_packet)
    {
        return NULL;
    }

    free(packet);
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
    char *destination_address = "ff0:babe:cafe::1";
    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(struct sockaddr_in6));
    
    err = inet_pton(AF_INET6, destination_address, &dst.sin6_addr.s6_addr);
    if (err == 0)
    {
        perror("IPv6 destination");
        exit(EXIT_FAILURE);
    }

    print_bft(bier);
    char dummy_payload[10];
    memset(dummy_payload, 1, sizeof(dummy_payload));
    uint64_t bitstring = 0xf;

    while (1)
    {
        my_packet_t *my_packet = create_bier_ipv6_from_payload(&bitstring, 64, &local, &dst, sizeof(dummy_payload), dummy_payload);
        if (!my_packet)
        {
            break;
        }
        fprintf(stderr, "Sending a new packet\n");
        err = bier_processing(my_packet->packet, my_packet->packet_length, socket_fd, bier);
        if (err < 0)
        {
            fprintf(stderr, "Error when processing the BIER packet at the sender... exiting...\n");
            my_packet_free(my_packet);
            break;
        }
        fprintf(stderr, "Sent a new packet!\n");
        my_packet_free(my_packet);
        sleep(interval);
    }

    // Free entire system
    fprintf(stderr, "Closing the program\n");
    free_bier_bft(bier);
    close(socket_fd);
    exit(EXIT_FAILURE);
}
