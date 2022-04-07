#include "../include/bier.h"

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

void print_bitstring_message(char *message, uint64_t *bitstring_ptr, uint32_t bitstring_max_idx)
{
    printf("%s ", message);
    for (int i = bitstring_max_idx - 1; i >= 0; --i)
    {
        printf("%lx ", bitstring_ptr[i]);
    }
    printf("\n");
}

void print_bft(bier_internal_t *bft)
{
    printf("=== Summary ===\n");
    printf("Local BFR ID: %d\n", bft->local_bfr_id);
    printf("Bitstring length: %d\n", bft->bitstring_length);
    printf("BFT:\n");
    uint32_t bitstring_max_idx = bft->bitstring_length / 64;
    for (int i = 0; i < bft->nb_bft_entry; ++i)
    {
        bier_bft_entry_t *bft_entry = bft->bft[i];
        printf("    #%u: ID=%u, ", i, bft_entry->bfr_id);
        for (uint32_t j = 0; j < bitstring_max_idx; ++j)
        {
            printf("%lx ", bft_entry->forwarding_bitmask[j]);
        }
        printf("\n");
    }
}

bier_bft_entry_t *parse_line(char *line_config, uint32_t bitstring_length)
{
    size_t line_length = strlen(line_config);
    char delim[] = " ";

    bier_bft_entry_t *bier_entry = malloc(sizeof(bier_bft_entry_t));
    if (!bier_entry)
    {
        fprintf(stderr, "Cannot allocate memory for the bier entry\n");
        perror("malloc");
        return NULL;
    }
    memset(bier_entry, 0, sizeof(bier_bft_entry_t));

    bier_entry->forwarding_bitmask = (uint64_t *)malloc(bitstring_length);
    if (!bier_entry->forwarding_bitmask)
    {
        perror("malloc forwarding bitmask");
        free(bier_entry);
        return NULL;
    }
    memset(bier_entry->forwarding_bitmask, 0, bitstring_length);

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
    // Parse the string line a bit at a time because it may be too long to hold in a single number
    uint64_t bitstring_iter = 0;
    uint64_t bitstring_word_iter = 0;
    int word_length = strlen(ptr);
    for (int i = word_length - 1; i >= 0; --i)
    {
        if (bitstring_iter >= 64)
        {
            bitstring_iter = 0;
            ++bitstring_word_iter;
        }
        char c = ptr[i];
        if (c == '1')
        {
            bier_entry->forwarding_bitmask[bitstring_word_iter] += 1 << bitstring_iter;
        }
        ++bitstring_iter;
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

    bier_entry->bfr_id = bfr_id;
    bier_entry->bfr_nei_addr = bfr_nei_addr;

    return bier_entry;

empty_string:
    fprintf(stderr, "Empty string: not complete line\n");
    if (bier_entry)
    {
        if (bier_entry->forwarding_bitmask)
        {
            free(bier_entry->forwarding_bitmask);
        }
        free(bier_entry);
    }
    return NULL;
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
    if (bft->socket >= 0)
    {
        close(bft->socket);
    }
    free(bft);
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
        return NULL;
    }
    memset(bier_bft, 0, sizeof(bier_internal_t));
    bier_bft->socket = -1;

    ssize_t readed = 0;
    char *line = NULL;
    size_t len = 0;

    // First line is the local address
    if ((readed = getline(&line, &len, file)) == -1)
    {
        fprintf(stderr, "Cannot get local address line\n");
        free(bier_bft);
        return NULL;
    }

    // The last byte is the '\n' => must erase it by inserting a 0
    line[readed - 1] = '\0';
    struct in6_addr local_addr = {};
    if (inet_pton(AF_INET6, line, local_addr.s6_addr) != 1)
    {
        fprintf(stderr, "Cannot convert the local address: %s\n", line);
        free(bier_bft);
        return NULL;
    }
    if ((readed = getline(&line, &len, file)) == -1)
    {
        fprintf(stderr, "Cannot get number of entries line\n");
        free(bier_bft);
        return NULL;
    }
    int nb_bft_entry = atoi(line);
    if (nb_bft_entry == 0)
    {
        fprintf(stderr, "Cannot convert to nb bft entry: %s\n", line);
        free(bier_bft);
        return NULL;
    }

    bier_bft->nb_bft_entry = nb_bft_entry;

    // We can create the array of entries for the BFT
    bier_bft->bft = malloc(sizeof(bier_bft_entry_t *) * nb_bft_entry);
    if (!bier_bft->bft)
    {
        fprintf(stderr, "Cannot malloc the bft!\n");
        free(bier_bft);
        return NULL;
    }
    memset(bier_bft->bft, 0, sizeof(bier_bft_entry_t *) * nb_bft_entry);

    // Make room for the bitstring
    // According to RFC8296, it can be up to 4096 bits
    uint32_t bitstring_length = 64;
    while (bitstring_length < nb_bft_entry)
    {
        bitstring_length <<= 1;
        if (bitstring_length > 4096)
        {
            fprintf(stderr, "Too long bitstring length\n");
            free(bier_bft->bft);
            free(bier_bft);
            return NULL;
        }
    }
    bier_bft->bitstring_length = bitstring_length;

    // The BFR ID of the local router
    if ((readed = getline(&line, &len, file)) == -1)
    {
        fprintf(stderr, "Cannot get the local BFR ID\n");
        free(bier_bft->bft);
        free(bier_bft);
        return NULL;
    }
    int local_bfr_id = atoi(line);
    if (local_bfr_id == 0)
    {
        fprintf(stderr, "Cannot convert to local BFR ID: %s\n", line);
        free(bier_bft->bft);
        free(bier_bft);
        return NULL;
    }
    bier_bft->local_bfr_id = local_bfr_id;

    // Fill in the BFT with the remaining of the file
    while ((readed = getline(&line, &len, file)) != -1)
    {
        bier_bft_entry_t *bft_entry = parse_line(line, bitstring_length);
        if (!bft_entry)
        {
            fprintf(stderr, "Cannot parse line: %s\n", line);
            free_bier_bft(bier_bft);
            return NULL;
        }
        bier_bft->bft[bft_entry->bfr_id - 1] = bft_entry; // bfr_id is one_indexed
    }

    // Open raw socket to forward the packets
    bier_bft->socket = socket(AF_INET6, SOCK_RAW, 253);
    if (bier_bft->socket < 0)
    {
        perror("socket BFT");
        free_bier_bft(bier_bft);
        return NULL;
    }

    struct sockaddr_in6 local_router = {
        .sin6_family = AF_INET6,
    };
    memcpy(bier_bft->local.sin6_addr.s6_addr, local_addr.s6_addr, sizeof(local_addr.s6_addr));

    if (bind(bier_bft->socket, (struct sockaddr *)&local_router, sizeof(local_router)) < 0)
    {
        perror("Bind local router");
        free_bier_bft(bier_bft);
        return NULL;
    }

    return bier_bft;
}

void update_bitstring(uint64_t *bitstring_ptr, bier_internal_t *bft, uint32_t bfr_idx, bitstring_operation op)
{
    uint32_t bitstring_max_idx = bft->bitstring_length / 64;
    for (uint32_t i = 0; i < bitstring_max_idx; ++i)
    {
        uint64_t bitstring = be64toh(bitstring_ptr[i]);
        uint64_t bitmask = bft->bft[bfr_idx]->forwarding_bitmask[i];
        switch (op)
        {
        case bitwise_u64_and:
            bitstring &= bitmask;
            break;
        case bitwise_u64_and_not:
            bitstring &= ~bitmask;
            break;
        }
        bitstring_ptr[i] = htobe64(bitstring);
    }
}

int bier_processing(uint8_t *buffer, size_t buffer_length, bier_internal_t *bft, bier_local_processing_t *bier_local_processing)
{
    // As specified in RFC 8296, we cannot rely on the `bsl` field of the BIER header
    // but we must know with the bift_id the true BSL
    uint32_t bift_id = get_bift_id(buffer);
    if (bift_id != 1 && 0)
    {
        fprintf(stderr, "Wrong BIFT_ID: must drop the packet: %u", bift_id);
        return -1;
    }

    // Remain as general as possible: handle all bitstring length
    uint32_t bitstring_max_idx = bft->bitstring_length / 64; // In 64 bits words
    uint32_t bitstring_length = bft->bitstring_length / 8;   // In bytes

    // RFC 8279
    uint32_t idx_bfr = 0;
    uint64_t *bitstring_ptr = get_bitstring_ptr(buffer);
    for (int bitstring_idx = bitstring_max_idx - 1; bitstring_idx >= 0; --bitstring_idx)
    {
        if (idx_bfr >= bft->nb_bft_entry)
        {
            fprintf(stderr, "There seems to be an error. The packet bitstring contains a bit set to true that is not mapped to a known BFR in the BFT.\n");
            return -1;
        }
        uint64_t bitstring = be64toh(bitstring_ptr[bitstring_idx]);
        // printf("LE BITSTRING index %u %lu\n", bitstring_idx, bitstring);

        // Use modulo operation for non-zero uint64_t words
        uint32_t idx_bfr_word = idx_bfr % 64;
        while ((bitstring >> idx_bfr_word) > 0)
        {
            if (idx_bfr >= bft->nb_bft_entry)
            {
                fprintf(stderr, "There seems to be an error. The packet bitstring contains a bit set to true that is not mapped to a known BFR in the BFT.\n");
                return -1;
            }
            if ((bitstring >> idx_bfr_word) & 1) // The current lowest-order bit is set: this BFER must receive a copy
            {
                // Here we use tje true idx_bfr because we do not use it as index for a table
                if (idx_bfr == bft->local_bfr_id - 1)
                {
                    fprintf(stderr, "Received a packet for local router %d!\n", bft->local_bfr_id);
                    fprintf(stderr, "Calling local processing function\n");
                    bier_local_processing->local_processing_function(buffer, buffer_length, 12 + bitstring_length, bier_local_processing->args);
                    update_bitstring(bitstring_ptr, bft, idx_bfr, bitwise_u64_and_not);
                    bitstring = be64toh(bitstring_ptr[bitstring_idx]);
                    ++idx_bfr;
                    idx_bfr_word = idx_bfr % 64;
                    continue;
                }
                fprintf(stderr, "Send a copy to %u\n", idx_bfr + 1);

                uint8_t packet_copy[buffer_length]; // Room for the IPv6 header
                memset(packet_copy, 0, sizeof(packet_copy));

                uint8_t *bier_header = &packet_copy[0];
                memcpy(bier_header, buffer, buffer_length);

                uint64_t bitstring_copy[bitstring_max_idx];
                memcpy(bitstring_copy, bitstring_ptr, sizeof(uint64_t) * bitstring_max_idx);
                // print_bitstring_message("Bitstring value copy is", bitstring_copy, bitstring_max_idx);
                update_bitstring(bitstring_copy, bft, idx_bfr, bitwise_u64_and);
                set_bitstring_ptr(packet_copy, bitstring_copy, bitstring_max_idx);
                // print_bitstring_message("Bitstring value copy is now", bitstring_copy, bitstring_max_idx);

                // Send copy
                bier_bft_entry_t *bft_entry = bft->bft[idx_bfr];
                int err = sendto(bft->socket, packet_copy, sizeof(packet_copy), 0, (struct sockaddr *)&bft_entry->bfr_nei_addr, sizeof(bft_entry->bfr_nei_addr));
                if (err < 0)
                {
                    perror("sendto");
                    return -1;
                }
                fprintf(stderr, "Sent packet\n");
                update_bitstring(bitstring_ptr, bft, idx_bfr, bitwise_u64_and_not);
                bitstring = be64toh(bitstring_ptr[bitstring_idx]);
                // printf("Bitstring is now %lu\n", bitstring);
            }
            ++idx_bfr; // Keep track of the index of the BFER to get the correct entry of the BFT
            idx_bfr_word = idx_bfr % 64;
        }
    }
    return 0;
}
