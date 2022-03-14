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
        return NULL;
    }
    memset(bier_bft, 0, sizeof(bier_internal_t));

    ssize_t readed = 0;
    char *line = NULL;
    size_t len = 0;

    // First line is the local address
    if ((readed = getline(&line, &len, file)) == -1)
    {
        fprintf(stderr, "Cannot get local address line\n");
        // goto free_bft;
        return NULL;
    }

    // The last byte is the '\n' => must erase it by inserting a 0
    line[readed - 1] = '\0';
    if (inet_pton(AF_INET6, line, bier_bft->local.s6_addr) != 1)
    {
        fprintf(stderr, "Cannot convert the local address: %s\n", line);
        free(bier_bft);
        return NULL;
        // goto free_bft;
    }
    if ((readed = getline(&line, &len, file)) == -1)
    {
        fprintf(stderr, "Cannot get number of entries line\n");
        return NULL;
        // goto free_bft;
    }
    int nb_bft_entry = atoi(line);
    if (nb_bft_entry == 0)
    {
        fprintf(stderr, "Cannot convert to nb bft entry: %s\n", line);
        return NULL;
    }

    bier_bft->nb_bft_entry = nb_bft_entry;
    // We can create the array of entries for the BFT
    bier_bft->bft = malloc(sizeof(bier_bft_entry_t *) * nb_bft_entry);
    if (!bier_bft->bft)
    {
        fprintf(stderr, "Cannot malloc the bft!\n");
        return NULL;
        // goto free_bft;
    }
    memset(bier_bft->bft, 0, sizeof(bier_bft_entry_t *) * nb_bft_entry);

    // The BFR ID of the local router
    if ((readed = getline(&line, &len, file)) == -1)
    {
        fprintf(stderr, "Cannot get the local BFR ID\n");
        return NULL; // goto free_bft;
    }
    int local_bfr_id = atoi(line);
    if (local_bfr_id == 0)
    {
        fprintf(stderr, "Cannot convert to local BFR ID: %s\n", line);
        return NULL;
    }
    bier_bft->local_bfr_id = local_bfr_id;

    // Fill in the BFT with the remaining of the file
    while ((readed = getline(&line, &len, file)) != -1)
    {
        printf("Line is %s\n", line);
        bier_bft_entry_t *bft_entry = parse_line(line);
        if (!bft_entry)
        {
            printf("ERROR PARSE LINE\n");
            return NULL;
            // goto cleanup_bft_entry;
        }
        printf("DONC JE LE METS a index: %u\n", bft_entry->bfr_id - 1);
        bier_bft->bft[bft_entry->bfr_id - 1] = bft_entry; // bfr_id is one_indexed
        printf("LA VALEUR EST: %x\n", bier_bft->bft[bft_entry->bfr_id - 1]->forwarding_bitmask);
    }
    return bier_bft;
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
    printf("LE BITSTRING %u\n", bitstring);
    for (int i = 0; i < bft->nb_bft_entry; ++i)
    {
        printf("Is null %uth entry? %u\n", i, bft->bft[i] == NULL);
    }
    while ((bitstring >> idx_bfr) > 0)
    {
        if ((bitstring >> idx_bfr) & 1) // The current lowest-order bit is set: this BFER must receive a copy
        {
            if (idx_bfr == bft->local_bfr_id - 1)
            {
                fprintf(stderr, "Received a packet for local router %d!\n", bft->local_bfr_id);
                fprintf(stderr, "Cleaning the bit and doing nothing with it...\n");
                printf("Params %u %u", idx_bfr, bft->nb_bft_entry);
                bier_bft_entry_t *bbb = bft->bft[idx_bfr];
                printf("TEEEST %u", bbb == NULL);
                bitstring &= ~bft->bft[idx_bfr]->forwarding_bitmask;
                continue;
            }
            printf("COUCOU3\n");
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
    return 0;
}