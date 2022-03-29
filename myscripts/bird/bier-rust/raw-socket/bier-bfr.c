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

    print_bft(bier);

    uint16_t buffer_size = 1500;
    uint8_t *buffer = (uint8_t *)malloc(sizeof(uint8_t) * buffer_size);
    while (1)
    {
        memset(buffer, 0, sizeof(uint8_t) * buffer_size);
        size_t length = recv(socket_fd, buffer, sizeof(uint8_t) * buffer_size, 0);
        fprintf(stderr, "Received packet of length=%lu on router %d\n", length, bier->local_bfr_id);
        print_buffer(buffer, length);
        bier_processing(buffer, length, socket_fd, bier);
    }

    free(buffer);
    fprintf(stderr, "Closing the program on router %u\n", bier->local_bfr_id);
    free_bier_bft(bier);
    close(socket_fd);
}
