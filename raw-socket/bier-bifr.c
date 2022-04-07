#include "include/bier.h"
#include "include/bier-sender.h"
#include "include/local-processing.h"

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s [config_file] [interval - ms] [bitstring]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

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
    if (inet_pton(AF_INET6, local_addr, &raw_args.dst.sin6_addr.s6_addr) == 0)
    {
        perror("loopback address");
        exit(EXIT_FAILURE);
    }
    memcpy(&raw_args.src.s6_addr, &raw_args.dst.sin6_addr.s6_addr, sizeof(raw_args.src.s6_addr));
    
    // TODO: able to change udp port (src, dst)
    int local_socket_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW);
    if (local_socket_fd < 0)
    {
        perror("socket loopback");
        exit(EXIT_FAILURE);
    }
    raw_args.raw_socket = local_socket_fd;
    bier_local_processing_t local_bier_processing = {};
    local_bier_processing.local_processing_function = &local_behaviour;
    local_bier_processing.args = (void *)&raw_args;

    print_bft(bier);
    char dummy_payload[10];
    memset(dummy_payload, 1, sizeof(dummy_payload));

    // Create the BIER packet header
    bier_header_t *bh = init_bier_header(&bitstring_arg, 64, 6);
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
}