#include "include/bier.h"
#include "include/local-processing.h"

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

    // Local router behaviour
    raw_socket_arg_t raw_args;
    memset(&raw_args, 0, sizeof(raw_socket_arg_t));
    raw_args.dst.sin6_family = AF_INET6;
    memcpy(&raw_args.dst.sin6_addr, &bier->local, 16);
    memcpy(&raw_args.src.s6_addr, &raw_args.dst.sin6_addr.s6_addr, sizeof(raw_args.src.s6_addr));

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
    local_bier_processing.local_processing_function = &local_behaviour;
    local_bier_processing.args = (void *)&raw_args;

    print_bft(bier);

    uint16_t buffer_size = 1500;
    uint8_t *buffer = (uint8_t *)malloc(sizeof(uint8_t) * buffer_size);
    struct sockaddr_in6 remote = {};
    socklen_t remote_len = sizeof(remote);
    while (1)
    {
        memset(buffer, 0, sizeof(uint8_t) * buffer_size);
        size_t length = recvfrom(bier->socket, buffer, sizeof(uint8_t) * buffer_size, 0, (struct sockaddr *)&remote, &remote_len);
        fprintf(stderr, "Received packet of length=%lu on router %d\n", length, bier->local_bfr_id);
        print_buffer(buffer, length);
        raw_args.src = remote.sin6_addr;
        char buf[100];
        inet_ntop(AF_INET6, &remote, buf, sizeof(remote));
        fprintf(stderr, "src %s\n", buf);
        bier_processing(buffer, length, bier, &local_bier_processing);
    }

    free(buffer);
    fprintf(stderr, "Closing the program on router %u\n", bier->local_bfr_id);
    free_bier_bft(bier);
}
