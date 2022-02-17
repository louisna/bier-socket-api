#include "include/bier-bfr.h"

void print_bier_bft(bier_internal_t *bft)
{
    char addr_str[INET6_ADDRSTRLEN];
    for (int i = 0; i < bft->nb_bft_entry; ++i)
    {
        memset(addr_str, 0, sizeof(char) * INET6_ADDRSTRLEN);
        if (inet_ntop(AF_INET6, &bft->bft[i]->bfr_nei_addr, addr_str, INET6_ADDRSTRLEN) == NULL)
        {
            perror("inet_ntop");
        }
        printf("Entry #%u: bfr id=%u, fw bm=%x, addr: %s\n", i, bft->bft[i]->bfr_id, bft->bft[i]->forwarding_bitmask, addr_str);
    }
}

void clean_line_break(char *line)
{
    int i = 0;
    char c = line[i];
    while (c != '\0')
    {
        if (c == '\n')
        {
            line[i] = '\0';
            return;
        }
        c = line[++i];
    }
}

void free_bier_bft(bier_internal_t *bft)
{
    for (int i = 0; i < bft->nb_bft_entry; ++i)
    {
        if (bft->bft[i])
        {
            free(bft->bft[i]);
        }
    }
    free(bft->bft);
    free(bft);
}

// TODO: to optimize, not have multiple sockaddr_in6 for the same neighbour!
bier_bft_entry_t *parse_line(char *line_config)
{
    size_t line_length = strlen(line_config);
    char delim[] = " ";

    // First is the BFR ID
    char *ptr = strtok(line_config, delim);
    if (ptr == NULL)
    {
        goto empty_string;
    }
    int bfr_id = atoi(ptr);
    if (bfr_id == 0)
    {
        fprintf(stderr, "Cannot convert BFR ID: %s\n", ptr);
        return NULL;
    }

    ptr = strtok(NULL, delim);
    if (ptr == NULL)
    {
        goto empty_string;
    }
    int forwarding_bitmask = strtoul(ptr, NULL, 2);
    if (forwarding_bitmask == 0)
    {
        fprintf(stderr, "Cannot convert forwarding bitmask: %s\n", ptr);
        return NULL;
    }

    ptr = strtok(NULL, delim);
    if (ptr == NULL)
    {
        goto empty_string;
    }
    struct sockaddr_in6 bfr_nei_addr;
    memset(&bfr_nei_addr, 0, sizeof(struct sockaddr_in6));
    bfr_nei_addr.sin6_family = AF_INET6;
    // Must also clean the '\n' of the string
    clean_line_break(ptr);
    if (inet_pton(AF_INET6, ptr, bfr_nei_addr.sin6_addr.s6_addr) != 1)
    {
        fprintf(stderr, "Cannot convert neighbour address: %s\n", ptr);
        perror("inet_ntop bfr_nei_addr");
        return NULL;
    }

    bier_bft_entry_t *bier_entry = malloc(sizeof(bier_bft_entry_t));
    if (!bier_entry)
    {
        fprintf(stderr, "Cannot allocate memory for the bier entry\n");
        perror("malloc");
        return NULL;
    }
    memset(bier_entry, 0, sizeof(bier_bft_entry_t));
    bier_entry->bfr_id = bfr_id;
    bier_entry->forwarding_bitmask = forwarding_bitmask;
    bier_entry->bfr_nei_addr = bfr_nei_addr;

    return bier_entry;

empty_string:
    fprintf(stderr, "Empty string: not complete line\n");
    return NULL;
}

// TODO: multiple checks:
//   * do we read exactly once each entry?
//   * do we have all entries?
bier_internal_t *read_config_file(char *config_filepath)
{
    FILE *file = fopen(config_filepath, "r");
    if (!file)
    {
        fprintf(stderr, "Impossible to open the config file: %s\n", config_filepath);
        return NULL;
    }

    bier_internal_t *bier_bft = malloc(sizeof(bier_internal_t));
    if (!bier_bft)
    {
        fprintf(stderr, "Cannot malloc\n");
        perror("malloc-config");
        goto close_file;
    }
    memset(bier_bft, 0, sizeof(bier_internal_t));
    
    ssize_t read = 0;
    char *line = NULL;
    size_t len = 0;
    
    // First line is the local address
    if ((read = getline(&line, &len, file)) == -1)
    {
        fprintf(stderr, "Cannot get local address line\n");
        goto free_bft;
    }
    // The last byte is the '\n' => must erase it by inserting a 0
    line[read - 1] = '\0';
    if (inet_pton(AF_INET6, line, bier_bft->local.s6_addr) != 1)
    {
        fprintf(stderr, "Cannot convert the local address: %s\n", line);

        goto free_bft;
    }
    
    if ((read = getline(&line, &len, file)) == -1)
    {
        fprintf(stderr, "Cannot get number of entries line\n");
        goto free_bft;
    }
    int nb_bft_entry = atoi(line);
    if (nb_bft_entry == 0)
    {
        fprintf(stderr, "Cannot convert to nb bft entry: %s\n", line);
    }
    bier_bft->nb_bft_entry = nb_bft_entry;
    
    // We can create the array of entries for the BFT
    bier_bft->bft = malloc(sizeof(bier_bft_entry_t *) * nb_bft_entry);
    if (!bier_bft->bft)
    {
        fprintf(stderr, "Cannot malloc the bft!\n");
        goto free_bft;
    }
    memset(bier_bft->bft, 0, sizeof(bier_bft_entry_t *) * nb_bft_entry);

    // The BFR ID of the local router
    if ((read = getline(&line, &len, file)) == -1)
    {
        fprintf(stderr, "Cannot get the local BFR ID\n");
        goto free_bft;
    }
    int local_bfr_id = atoi(line);
    if (local_bfr_id == 0)
    {
        fprintf(stderr, "Cannot convert to local BFR ID: %s\n", line);
    }
    bier_bft->local_bfr_id = local_bfr_id;

    // Fill in the BFT with the remaining of the file
    while ((read = getline(&line, &len, file)) != -1)
    {
        printf("Line is %s\n", line);
        bier_bft_entry_t *bft_entry = parse_line(line);
        if (!bft_entry)
        {
            goto cleanup_bft_entry;
        }
        bier_bft->bft[bft_entry->bfr_id - 1] = bft_entry; // bfr_id is one_indexed
    }

    return bier_bft;

cleanup_bft_entry:
    for (int i = 0; i < bier_bft->nb_bft_entry; ++i)
    {
        if (bier_bft->bft[i])
        {
            free(bier_bft->bft[i]);
        }
    }
free_bft:
    if (bier_bft->bft)
    {
        free(bier_bft->bft);
    }
    free(bier_bft);
close_file:
    fclose(file);
    if (line)
    {
        free(line);
    }
    return NULL;
}

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

int bier_processing(uint8_t *buffer, size_t buffer_length, int socket_fd, bier_internal_t *bft)
{
    // As specified in RFC 8296, we cannot rely on the `bsl` field of the BIER header
    // but we must know with the bift_id the true BSL
    uint32_t bift_id = get_bift_id(buffer);
    if (bift_id != 1 && 0)
    {
        fprintf(stderr, "Wrong BIFT_ID: must drop the packet: %u", bift_id);
        return -1;
    }

    // Here we can assume that we know the BSL: 0 meaning that the bitstring has a length of 4 bytes
    uint32_t bitstring_length = 4; // Bytes
    uint32_t bitstring_idx = 0;    // Offset in the bitstring
    uint32_t bitstring = get_bitstring(buffer, bitstring_idx);

    // RFC 8279
    uint32_t idx_bfr = 0;
    while ((bitstring >> idx_bfr) > 0)
    {
        if ((bitstring >> idx_bfr) & 1) // The current lowest-order bit is set: this BFER must receive a copy
        {
            if (idx_bfr == bft->local_bfr_id - 1)
            {
                fprintf(stderr, "Received a packet for local router %d!\n", bft->local_bfr_id);
                fprintf(stderr, "Cleaning the bit and doing nothing with it...\n");
                bitstring &= ~bft->bft[idx_bfr]->forwarding_bitmask;
                continue;
            }
            fprintf(stderr, "Send a copy to %u\n", idx_bfr + 1);

            // Get the correct entry in the BFT
            bier_bft_entry_t *bft_entry = bft->bft[idx_bfr];

            uint8_t packet_copy[buffer_length]; // Room for the IPv6 header
            memset(packet_copy, 0, sizeof(packet_copy));

            uint8_t *bier_header = &packet_copy[0];
            memcpy(bier_header, buffer, buffer_length);

            uint32_t bitstring_copy = bitstring;
            printf("BitString value is: %x\n", bitstring_copy);
            bitstring_copy &= bft_entry->forwarding_bitmask;
            set_bitstring(bier_header, 0, bitstring_copy);
            printf("BitString value is now: %x\n", bitstring_copy);

            // Send copy
            int err = 0; // encapsulate_ipv6(packet_copy, buffer_length);
            if (err < 0)
            {
                return err;
            }

            err = sendto(socket_fd, packet_copy, sizeof(packet_copy), 0, (struct sockaddr *)&bft_entry->bfr_nei_addr, sizeof(bft_entry->bfr_nei_addr));
            if (err < 0)
            {
                perror("sendto");
                return -1;
            }
            printf("Sent packet\n");
            bitstring &= ~bft_entry->forwarding_bitmask;
        }
        ++idx_bfr; // Keep track of the index of the BFER to get the correct entry of the BFT
    }
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
    print_bier_bft(bier);

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

    uint8_t buffer[4096];

    memset(buffer, 0, sizeof(buffer));
    size_t length = recv(socket_fd, buffer, 4096, 0);
    printf("Length=%lu on router %d\n", length, bier->local_bfr_id);
    print_buffer(buffer, length);
    bier_processing(buffer, length, socket_fd, bier);

    fprintf(stderr, "Closing the program\n");
    free_bier_bft(bier);
    close(socket_fd);
    
}