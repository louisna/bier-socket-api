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
    udp_header->uh_dport = htons(5000);
    // udp_header->uh_sport = 5678;
    udp_header->uh_ulen = htons(udp_header_length + payload_length);

    // Payload
    uint8_t *packet_payload = &packet[ipv6_header_length + udp_header_length];
    memcpy(packet_payload, payload, sizeof(uint8_t) * payload_length);

    my_packet_t *my_packet = encap_bier_packet(bh, packet_total_length, packet);
    if (!my_packet)
    {
        return NULL;
    }

    free(packet);
    return my_packet;
}

/**
 * @brief This is an example of a function that handles a BIER packet whose destination is the local router processing it.
 * It just transmits the content of the BIER packet to the socket given in argument
 *
 * @param bier_packet BIER packet buffer. The BIER header is still in the buffer in case the application needs to retrieve information from it
 * @param packet_length the length of the packet buffer
 * @param bier_header_length the length of the BIER header (at the beginning of the packet)
 * @param args additional arguments given to the function. The function is responsible to know how to parse these arguments
 */
int send_payload(bier_internal_t *bier, const uint64_t *bitstring, uint32_t bitstring_length, const void *payload, size_t payload_length)
{
    // Are the two following lines mandatory?
    uint8_t buffer[payload_length];
    memcpy(buffer, payload, payload_length);

    // Destination of the multicast packet embedded in the BIER packet
    // This must be a multicast address
    char *destination_address = "ff0:babe:cafe::1";
    struct sockaddr_in6 dst = {};
    if (inet_pton(AF_INET6, destination_address, &dst.sin6_addr.s6_addr) == 0)
    {
        perror("IPv6 destination");
        exit(EXIT_FAILURE);
    }

    int err = bind(bier->socket, (struct sockaddr *)&bier->local, sizeof(bier->local));
    if (err < 0)
    {
        perror("bind local");
        exit(EXIT_FAILURE);
    }

    // Local router behaviour
    raw_socket_arg_t raw_args = {};
    raw_args.local.sin6_family = AF_INET6;
    // raw_args.local.sin6_port = htons(5000);
    // inet_pton(AF_INET6, "::1", &raw_args.local.sin6_addr);
    memcpy(&raw_args.local.sin6_addr, &bier->local, sizeof(bier->local));

    // TODO: able to change udp port (src, dst)
    // TODO: should be outside of the BIER sender processing
    int local_socket_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW);
    if (local_socket_fd < 0)
    {
        perror("socket loopback");
        return -1;
    }
    raw_args.raw_socket = local_socket_fd;
    raw_args.src = bier->local.sin6_addr;
    bier_local_processing_t local_bier_processing = {};
    local_bier_processing.local_processing_function = &send_to_raw_socket;
    local_bier_processing.args = (void *)&raw_args;

    // Create the BIER packet header
    bier_header_t *bh = init_bier_header(bitstring, bitstring_length, 6);
    if (!bh)
    {
        return -1;
    }

    my_packet_t *my_packet = create_bier_ipv6_from_payload(bh, &bier->local, &dst, payload_length, (uint8_t *)buffer);
    if (!my_packet)
    {
        return -1;
    }
    fprintf(stderr, "Sending a new packet\n");
    if (bier_processing(my_packet->packet, my_packet->packet_length, bier, &local_bier_processing) < 0)
    {
        fprintf(stderr, "Error when processing the BIER packet at the sender... exiting...\n");
        my_packet_free(my_packet);
        return -1;
    }
    fprintf(stderr, "Sent a new packet!\n");
    my_packet_free(my_packet);

    return 0;
}

/*int main(int argc, char *argv[])
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

    // Destination of the multicast packet embedded in the BIER packet
    // This must be a multicast address
    char *destination_address = "ff0:babe:cafe::1";
    struct sockaddr_in6 dst = {};

    int err = inet_pton(AF_INET6, destination_address, &dst.sin6_addr.s6_addr);
    if (err == 0)
    {
        perror("IPv6 destination");
        exit(EXIT_FAILURE);
    }

    // Local router behaviour
    raw_socket_arg_t raw_args = {};
    char *local_addr = "::1"; // Send to loopback the packets belonging to the router
    memset(&raw_args.local, 0, sizeof(struct sockaddr_in6));
    if (inet_pton(AF_INET6, local_addr, raw_args.local.sin6_addr.s6_addr) == 0)
    {
        perror("loopback address");
        exit(EXIT_FAILURE);
    }
    // TODO: able to change udp port (src, dst)
    int local_socket_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW);
    if (local_socket_fd < 0)
    {
        perror("socket loopback");
        exit(EXIT_FAILURE);
    }
    raw_args.raw_socket = local_socket_fd;
    bier_local_processing_t local_bier_processing = {};
local_bier_processing.local_processing_function = &send_to_raw_socket;
    local_bier_processing.args = (void *)&raw_args;

    print_bft(bier);
    char dummy_payload[10];
    memset(dummy_payload, 1, sizeof(dummy_payload));
    uint64_t bitstring = 0xf;

    // Create the BIER packet header
    bier_header_t *bh = init_bier_header(&bitstring, 64, 6);
    if (!bh)
    {
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        my_packet_t *my_packet = create_bier_ipv6_from_payload(bh, &bier->local, &dst, sizeof(dummy_payload), dummy_payload);
        if (!my_packet)
        {
            break;
        }
        fprintf(stderr, "Sending a new packet\n");
        err = bier_processing(my_packet->packet, my_packet->packet_length, bier, &local_bier_processing);
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
    exit(EXIT_FAILURE);
}*/
