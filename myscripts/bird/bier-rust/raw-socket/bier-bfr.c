#include "include/bier.h"

// void print_bier_bft(bier_internal_t *bft)
// {
//     char addr_str[INET6_ADDRSTRLEN];
//     for (int i = 0; i < bft->nb_bft_entry; ++i)
//     {
//         memset(addr_str, 0, sizeof(char) * INET6_ADDRSTRLEN);
//         if (inet_ntop(AF_INET6, &bft->bft[i]->bfr_nei_addr, addr_str, INET6_ADDRSTRLEN) == NULL)
//         {
//             perror("inet_ntop");
//         }
//         printf("Entry #%u: bfr id=%u, fw bm=%x, addr: %s\n", i, bft->bft[i]->bfr_id, bft->bft[i]->forwarding_bitmask, addr_str);
//     }
// }

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

int encapsulate_ipv6(uint8_t *buffer, uint32_t buffer_length)
{
    struct ip6_hdr *iphdr;
    iphdr = (struct ip6_hdr *)buffer;
    iphdr->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);
    iphdr->ip6_nxt = 253; // Nxt hdr = UDP
    iphdr->ip6_hops = 11;
    printf("Packet length is: %u %d", buffer_length, buffer_length);
    iphdr->ip6_plen = htons((uint16_t)buffer_length); // Changed later

    // IPv6 Source address
    struct sockaddr_in6 src;
    memset(&src, 0, sizeof(struct sockaddr_in6));
    src.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, "::3", src.sin6_addr.s6_addr) != 1)
    {
        perror("inet_ntop src");
        return -1;
    }
    bcopy(&src.sin6_addr, &(iphdr->ip6_src), 16);

    // IPv6 Destination address
    struct sockaddr_in6 dst;
    memset(&dst, 0, sizeof(struct sockaddr_in6));
    dst.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, "::1", dst.sin6_addr.s6_addr) != 1)
    {
        perror("inet_ntop dst");
        return -1;
    }
    bcopy(&dst.sin6_addr, &(iphdr->ip6_dst), 16);

    return 0;
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
    // print_bier_bft(bier);
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

    // uint8_t buffer[4096];
    uint16_t buffer_size = 1500;
    uint8_t *buffer = (uint8_t *)malloc(sizeof(uint8_t) * buffer_size);
    while (1)
    {
        memset(buffer, 0, sizeof(uint8_t) * buffer_size);
        size_t length = recv(socket_fd, buffer, sizeof(uint8_t) * buffer_size, 0);
        printf("Length=%lu on router %d\n", length, bier->local_bfr_id);
        print_buffer(buffer, length);
        bier_processing(buffer, length, socket_fd, bier);
    }

    free(buffer);
    fprintf(stderr, "Closing the program\n");
    free_bier_bft(bier);
    close(socket_fd);
}
