#include "include/bier.h"

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

/*void send_to_raw_socket(const uint8_t *bier_packet, const uint32_t packet_length, const uint32_t bier_header_length, void *args)
{
    raw_socket_arg_t *raw_args = (raw_socket_arg_t *)args;
    const uint8_t *ipv6_packet = &bier_packet[bier_header_length];
    int err = sendto(raw_args->raw_socket, ipv6_packet, packet_length - bier_header_length, 0, (struct sockaddr *)&raw_args->local, sizeof(raw_args->local));
    if (err < 0)
    {
        perror("Cannot send using raw socket... ignoring");
    }
}*/

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];
    bier_internal_t *bier = read_config_file(filename);
    if (!bier)
    {
        exit(EXIT_FAILURE);
    }
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
    memcpy(&local.sin6_addr.s6_addr, bier->local.s6_addr, sizeof(bier->local.s6_addr));

    int err = bind(socket_fd, (struct sockaddr *)&local, sizeof(local));
    if (err < 0)
    {
        perror("bind local");
        exit(EXIT_FAILURE);
    }

    // Local router behaviour
    raw_socket_arg_t raw_args;
    memset(&raw_args, 0, sizeof(raw_socket_arg_t));
    /*char *local_addr = "::1"; // Send to loopback the packets belonging to the router
    memset(&raw_args.local, 0, sizeof(struct sockaddr_in6));

    if (inet_pton(AF_INET6, local_addr, &raw_args.local.sin6_addr.s6_addr) == 0)
    {
        perror("loopback address");
        exit(EXIT_FAILURE);
    }*/
    raw_args.local.sin6_family = AF_INET6;
    memcpy(&raw_args.local.sin6_addr, &bier->local, 16);

    // TODO: able to change udp port (src, dst)
    int local_socket_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_RAW);
    if (local_socket_fd < 0)
    {
        perror("socket loopback");
        exit(EXIT_FAILURE);
    }
    raw_args.raw_socket = local_socket_fd;
    bier_local_processing_t local_bier_processing;
    memset(&local_bier_processing, 0, sizeof(bier_local_processing_t));
    local_bier_processing.local_processing_function = &send_to_raw_socket;
    local_bier_processing.args = (void *)&raw_args;

    print_bft(bier);

    uint16_t buffer_size = 1500;
    uint8_t *buffer = (uint8_t *)malloc(sizeof(uint8_t) * buffer_size);
    while (1)
    {
        memset(buffer, 0, sizeof(uint8_t) * buffer_size);
        size_t length = recv(socket_fd, buffer, sizeof(uint8_t) * buffer_size, 0);
        fprintf(stderr, "Received packet of length=%lu on router %d\n", length, bier->local_bfr_id);
        print_buffer(buffer, length);
        bier_processing(buffer, length, socket_fd, bier, &local_bier_processing);
    }

    free(buffer);
    fprintf(stderr, "Closing the program on router %u\n", bier->local_bfr_id);
    free_bier_bft(bier);
    close(socket_fd);
}
