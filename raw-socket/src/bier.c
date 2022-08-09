#include "../include/bier.h"

#include "../include/qcbor-encoding.h"

void clean_line_break(char *line) {
    int i = 0;
    char c = line[i];
    while (c != '\0') {
        if (c == '\n') {
            line[i] = '\0';
            return;
        }
        c = line[++i];
    }
}

void print_bitstring_message(char *message, uint64_t *bitstring_ptr,
                             uint32_t bitstring_max_idx) {
    printf("%s ", message);
    for (int i = bitstring_max_idx - 1; i >= 0; --i) {
        printf("%lx ", bitstring_ptr[i]);
    }
    printf("\n");
}

void print_bft(bier_internal_t *bft) {
    printf("=== Summary ===\n");
    printf("Local BFR ID: %d\n", bft->local_bfr_id);
    printf("Bitstring length: %d\n", bft->bitstring_length);
    printf("BFT:\n");
    uint32_t bitstring_max_idx = bft->bitstring_length / 64;
    for (int i = 0; i < bft->nb_bft_entry; ++i) {
        bier_bft_entry_t *bft_entry = bft->bft[i];
        printf("    #%u: ID=%u", i + 1, bft_entry->bfr_id);
        if (bft_entry->nb_ecmp_entries > 1) {
            printf(" (ECMP=%u)", bft_entry->nb_ecmp_entries);
        }
        printf(": ");
        for (int ecmp_entry = 0; ecmp_entry < bft_entry->nb_ecmp_entries;
             ++ecmp_entry) {
            bier_bft_entry_ecmp_t *ecmp_map = bft_entry->ecmp_entry[ecmp_entry];
            for (uint32_t j = 0; j < bitstring_max_idx; ++j) {
                printf("%lx ", ecmp_map->forwarding_bitmask[j]);
            }
            if (ecmp_entry < bft_entry->nb_ecmp_entries - 1) {
                printf(", ");
            }
        }
        printf("\n");
    }
}

bier_bft_entry_t *parse_line(char *line_config, uint32_t bitstring_length) {
    size_t line_length = strlen(line_config);
    char delim[] = " ";

    bier_bft_entry_t *bier_entry = malloc(sizeof(bier_bft_entry_t));
    if (!bier_entry) {
        fprintf(stderr, "Cannot allocate memory for the bier entry\n");
        perror("malloc");
        return NULL;
    }
    memset(bier_entry, 0, sizeof(bier_bft_entry_t));

    // First is the BFR ID
    char *ptr = strtok(line_config, delim);
    if (ptr == NULL) {
        goto empty_string;
    }
    int bfr_id = atoi(ptr);
    if (bfr_id == 0) {
        fprintf(stderr, "Cannot convert BFR ID: %s\n", ptr);
        return NULL;
    }

    // Number of ECMP paths
    ptr = strtok(NULL, delim);
    if (ptr == NULL) {
        goto empty_string;
    }
    int nb_ecmp = atoi(ptr);
    if (nb_ecmp == 0) {
        fprintf(stderr, "Cannot convert the number of ECMP paths: %s\n", ptr);
        return NULL;
    }
    bier_entry->nb_ecmp_entries = nb_ecmp;
    bier_entry->ecmp_entry = (bier_bft_entry_ecmp_t **)malloc(
        sizeof(bier_bft_entry_ecmp_t *) * nb_ecmp);
    if (!bier_entry->ecmp_entry) {
        perror("malloc ecmp entry");
        return NULL;
    }

    // Create all ECMP entries for this destination BFR ID
    for (int i_ecmp = 0; i_ecmp < nb_ecmp; ++i_ecmp) {
        bier_bft_entry_ecmp_t *entry_ecmp =
            (bier_bft_entry_ecmp_t *)malloc(sizeof(bier_bft_entry_ecmp_t));
        if (!entry_ecmp) {
            fprintf(stderr, "Cannot allocate memory for the entry\n");
            return NULL;
        }
        memset(entry_ecmp, 0, sizeof(bier_bft_entry_ecmp_t));

        entry_ecmp->forwarding_bitmask = (uint64_t *)malloc(bitstring_length);
        if (!entry_ecmp->forwarding_bitmask) {
            perror("malloc forwarding bitmask");
            // TODO: free allocated memory
            return NULL;
        }
        memset(entry_ecmp->forwarding_bitmask, 0, sizeof(bitstring_length));

        ptr = strtok(NULL, delim);
        if (ptr == NULL) {
            goto empty_string;
        }

        // Parse the string line a bit at a time because it may be too long to
        // hold in a single number
        uint64_t bitstring_iter = 0;
        uint64_t bitstring_word_iter = 0;
        int word_length = strlen(ptr);
        for (int i = word_length - 1; i >= 0; --i) {
            if (bitstring_iter >= 64) {
                bitstring_iter = 0;
                ++bitstring_word_iter;
            }
            char c = ptr[i];
            if (c == '1') {
                entry_ecmp->forwarding_bitmask[bitstring_word_iter] +=
                    1 << bitstring_iter;
            }
            ++bitstring_iter;
        }

        ptr = strtok(NULL, delim);
        if (ptr == NULL) {
            goto empty_string;
        }
        struct sockaddr_in6 bfr_nei_addr;
        memset(&bfr_nei_addr, 0, sizeof(struct sockaddr_in6));
        bfr_nei_addr.sin6_family = AF_INET6;
        // Must also clean the '\n' of the string
        clean_line_break(ptr);
        if (inet_pton(AF_INET6, ptr, bfr_nei_addr.sin6_addr.s6_addr) != 1) {
            fprintf(stderr, "Cannot convert neighbour address: %s\n", ptr);
            perror("inet_ntop bfr_nei_addr");
            return NULL;
        }

        entry_ecmp->bfr_nei_addr = bfr_nei_addr;
        bier_entry->ecmp_entry[i_ecmp] = entry_ecmp;
    }
    bier_entry->bfr_id = bfr_id;

    return bier_entry;

empty_string:
    // TODO: adapt and clean that
    fprintf(stderr, "Empty string: not complete line\n");
    /*if (bier_entry)
    {
        if (bier_entry->forwarding_bitmask)
        {
            free(bier_entry->forwarding_bitmask);
        }
        free(bier_entry);
    }*/
    return NULL;
}

void free_bier_bft(bier_bift_t *bift) {
    for (int bift_id = 0; bift_id < bift->nb_bift; ++bift_id) {
        bier_internal_t *bft = bift->b[bift_id].bier;  // bier_bift[bift_id];
        for (int i = 0; i < bft->nb_bft_entry; ++i) {
            if (bft->bft[i]) {
                for (int j = 0; j < bft->bft[i]->nb_ecmp_entries; ++j) {
                    if (bft->bft[i]->ecmp_entry[j]) {
                        free(bft->bft[i]->ecmp_entry[j]);
                    }
                }
                free(bft->bft[i]);
            }
        }
        free(bft->bft);
        free(bft);
    }
    if (bift->socket >= 0) {
        close(bift->socket);
    }
    free(bift);
}

int fill_bier_internal_bier(FILE *file, bier_internal_t *bier_bft) {
    char *line = NULL;
    ssize_t readed = 0;
    size_t len = 0;

    if ((readed = getline(&line, &len, file)) == -1) {
        fprintf(stderr, "Cannot get number of entries line\n");
        free(bier_bft);
        return -1;
    }
    int nb_bft_entry = atoi(line);
    if (nb_bft_entry == 0) {
        fprintf(stderr, "Cannot convert to nb bft entry: %s\n", line);
        free(bier_bft);
        return -1;
    }

    bier_bft->nb_bft_entry = nb_bft_entry;

    // We can create the array of entries for the BFT
    bier_bft->bft = malloc(sizeof(bier_bft_entry_t *) * nb_bft_entry);
    if (!bier_bft->bft) {
        fprintf(stderr, "Cannot malloc the bft!\n");
        free(bier_bft);
        return -1;
    }
    memset(bier_bft->bft, 0, sizeof(bier_bft_entry_t *) * nb_bft_entry);

    // Make room for the bitstring
    // According to RFC8296, it can be up to 4096 bits
    uint32_t bitstring_length = 64;
    while (bitstring_length < nb_bft_entry) {
        bitstring_length <<= 1;
        if (bitstring_length > 4096) {
            fprintf(stderr, "Too long bitstring length\n");
            free(bier_bft->bft);
            free(bier_bft);
            return -1;
        }
    }
    bier_bft->bitstring_length = bitstring_length;

    //free(line);
    //line = NULL;

    // The BFR ID of the local router
    if ((readed = getline(&line, &len, file)) == -1) {
        fprintf(stderr, "Cannot get the local BFR ID\n");
        free(bier_bft->bft);
        free(bier_bft);
        return -1;
    }
    int local_bfr_id = atoi(line);
    if (local_bfr_id == 0) {
        fprintf(stderr, "Cannot convert to local BFR ID: %s\n", line);
        free(bier_bft->bft);
        free(bier_bft);
        return -1;
    }
    bier_bft->local_bfr_id = local_bfr_id;

    //free(line);
    //line = NULL;

    // Fill in the BFT
    for (int i = 0; i < nb_bft_entry; ++i) {
        if ((readed = getline(&line, &len, file)) == -1) {
            fprintf(stderr, "Cannot get line configuration\n");
            free(bier_bft->bft);
            free(bier_bft);
            return -1;
        }
        bier_bft_entry_t *bft_entry = parse_line(line, bitstring_length);
        if (!bft_entry) {
            fprintf(stderr, "Cannot parse line: %s\n", line);
            return -1;
        }
        bier_bft->bft[bft_entry->bfr_id - 1] =
            bft_entry;  // bfr_id is one_indexed
        
        //free(line);
        //line = NULL;
    }
    return 0;
}

int fill_bier_internal_bier_te(FILE *file, bier_te_internal_t *bier_internal) {
    char *line = NULL;
    ssize_t readed = 0;
    size_t len = 0;

    if ((readed = getline(&line, &len, file)) == -1) {
        fprintf(stderr, "Cannot get number of BP\n");
        return -1;
    }
    int nb_bp = atoi(line);
    if (nb_bp == 0) {
        fprintf(stderr, "Cannot convert to nb bp: %s\n", line);
        return -1;
    }

    uint32_t bitstring_length = 64;
    while (bitstring_length < nb_bp) {
        bitstring_length <<= 1;
        if (bitstring_length > 4096) {
            fprintf(stderr, "Too long bitstring length\n");
            return -1;
        }
    }
    bier_internal->bitstring_length = bitstring_length;
    bier_internal->global_bitstring =
        (uint64_t *)malloc(sizeof(uint64_t) * (bitstring_length / 64));
    if (!bier_internal->global_bitstring) {
        perror("Malloc bier internal global bitstring");
        return -1;
    }
    memset(bier_internal->global_bitstring, 0,
           sizeof(uint64_t) * (bitstring_length / 64));

    //free(line);
    //line = NULL;

    if ((readed = getline(&line, &len, file)) == -1) {
        fprintf(stderr, "Cannot get node bp id\n");
        return -1;
    }
    int node_bp_id = atoi(line);
    if (node_bp_id == 0) {
        fprintf(stderr, "Cannot convert to node bp id: %s\n", line);
        return -1;
    }

    bier_internal->local_bfr_id = node_bp_id;

    //free(line);
    //line = NULL;

    // Global bitstring
    if ((readed = getline(&line, &len, file)) == -1) {
        fprintf(stderr, "Cannot get global bitstring\n");
        return -1;
    }
    // Parse the string line a bit at a time because it may be too long to hold
    // in a single number
    uint64_t bitstring_iter = 0;
    uint64_t bitstring_word_iter = 0;
    int word_length = strlen(line);
    // Last char is a \n
    for (int i = word_length - 2; i >= 0; --i) {
        if (bitstring_iter >= 64) {
            bitstring_iter = 0;
            ++bitstring_word_iter;
        }
        char c = line[i];
        if (c == '1') {
            bier_internal->global_bitstring[bitstring_word_iter] +=
                1 << bitstring_iter;
        }
        ++bitstring_iter;
    }

    //free(line);
    //line = NULL;

    if ((readed = getline(&line, &len, file)) == -1) {
        fprintf(stderr, "Cannot get nb entries in the map\n");
        return -1;
    }
    int nb_entries = atoi(line);
    if (nb_entries == 0) {
        fprintf(stderr, "Cannot convert to nb entries in the map: %s\n", line);
        return -1;
    }
    bier_internal->nb_adjacencies = nb_entries;

    // Parsing the lines
    // It is only an array of sockaddr_in6. The index corresponds to the mapping
    // of the BP of the adjacency bit
    bier_internal->bfr_nei_addr =
        (struct sockaddr_in6 *)malloc(sizeof(struct sockaddr_in6) * nb_entries);
    if (!bier_internal->bfr_nei_addr) {
        perror("Malloc bier te bfr nei addr");
        return -1;
    }
    memset(bier_internal->bfr_nei_addr, 0,
           sizeof(struct sockaddr_in6) * nb_entries);

    bier_internal->adj_to_bp = (int *)malloc(sizeof(int) * nb_entries);
    if (!bier_internal->adj_to_bp) {
        perror("Malloc bier te adj to bp");
        return -1;
    }
    memset(bier_internal->adj_to_bp, 0, sizeof(int) * nb_entries);

    //free(line);
    //line = NULL;

    char delim[] = " ";
    for (int i = 0; i < nb_entries; ++i) {
        if ((readed = getline(&line, &len, file)) == -1) {
            fprintf(stderr, "Cannot get line\n");
            return -1;
        }
        char *ptr = strtok(line, delim);
        if (ptr == NULL) {
            fprintf(stderr, "Cannot get bier te idx\n");
            return -1;
        }
        int idx = atoi(ptr);
        if (idx == 0) {
            fprintf(stderr, "Cannot convert to idx bier te\n");
            return -1;
        }
        bier_internal->adj_to_bp[i] = idx;

        ptr = strtok(NULL, delim);
        if (ptr == NULL) {
            fprintf(stderr, "Cannot get bier te ECMP nb\n");
            return -1;
        }

        // No ECMP for now
        ptr = strtok(NULL, delim);
        if (ptr == NULL) {
            fprintf(stderr, "Cannot get neigh address bier te\n");
            return -1;
        }
        struct sockaddr_in6 bfr_nei_addr = {};
        bfr_nei_addr.sin6_family = AF_INET6;
        // Must also clean the '\n' of the string
        clean_line_break(ptr);
        if (inet_pton(AF_INET6, ptr, bfr_nei_addr.sin6_addr.s6_addr) != 1) {
            fprintf(stderr, "Cannot convert neighbour address bier te: %s\n",
                    ptr);
            perror("inet_ntop bfr_nei_addr");
            return -1;
        }
        bier_internal->bfr_nei_addr[i] = bfr_nei_addr;
    }
    return 0;
}

// TODO: multiple checks:
//   * do we read exactly once each entry?
//   * do we have all entries?
bier_bift_t *read_config_file(char *config_filepath) {
    FILE *file = fopen(config_filepath, "r");
    if (!file) {
        fprintf(stderr, "Impossible to open the config file: %s\n",
                config_filepath);
        return NULL;
    }

    bier_bift_t *bier_bift = malloc(sizeof(bier_bift_t));
    if (!bier_bift) {
        fprintf(stderr, "Cannot malloc\n");
        perror("malloc-config");
        return NULL;
    }
    memset(bier_bift, 0, sizeof(bier_bift_t));
    bier_bift->socket = -1;

    ssize_t readed = 0;
    char *line = NULL;
    size_t len = 0;

    // First line is the local address
    if ((readed = getline(&line, &len, file)) == -1) {
        fprintf(stderr, "Cannot get local address line\n");
        free(bier_bift);
        return NULL;
    }

    // The last byte is the '\n' => must erase it by inserting a 0
    line[readed - 1] = '\0';
    struct in6_addr local_addr = {};
    if (inet_pton(AF_INET6, line, local_addr.s6_addr) != 1) {
        fprintf(stderr, "Cannot convert the local address: %s\n", line);
        free(bier_bift);
        return NULL;
    }

    //free(line);
    //line = NULL;

    if ((readed = getline(&line, &len, file)) == -1) {
        fprintf(stderr, "Cannot get number of BIFTs line\n");
        free(bier_bift);
        return NULL;
    }
    // Number of different BIFT (each with an increasing ID for now)
    // TODO: generalize this
    int nb_bifts = atoi(line);
    printf("NB BIFT=%d\n", nb_bifts);
    if (nb_bifts == 0) {
        fprintf(stderr, "Cannot convert to nb bifts: %s\n", line);
    }
    bier_bift->nb_bift = nb_bifts;
    bier_bift->b =
        (bier_bift_type_t *)malloc(sizeof(bier_bift_type_t) * nb_bifts);
    if (!bier_bift->b) {
        perror("Malloc BIFTs");
        free(bier_bift);
        return NULL;
    }

    //free(line);
    //line = NULL;

    for (int bift_id = 0; bift_id < nb_bifts; ++bift_id) {
        if ((readed = getline(&line, &len, file)) == -1) {
            fprintf(stderr, "Cannot BIFT type line\n");
            free(bier_bift);
            return NULL;
        }
        int bift_type = atoi(line);
        if (bift_type == 0) {
            fprintf(stderr, "Cannot convert to BIFT type: %s\n", line);
            free(bier_bift->b);
            free(bier_bift);
            return NULL;
        }

        // TODO: continue process
        if (bift_type == BIER) {
            bier_internal_t *bier_internal =
                (bier_internal_t *)malloc(sizeof(bier_internal_t));
            if (!bier_internal) {
                perror("Malloc bier_internal");
                // TODO: free memory
                return NULL;
            }
            memset(bier_internal, 0, sizeof(bier_internal_t));
            bier_internal->bift_id =
                bift_id;  // TODO: this must be more general (include an ID in
                          // the configuration?)
            bier_bift->b[bift_id].bier = bier_internal;
            bier_bift->b[bift_id].t = BIER;
            if (fill_bier_internal_bier(file, bier_internal) != 0) {
                return NULL;
            }
        } else if (bift_type == BIER_TE) {
            bier_te_internal_t *bier_internal =
                (bier_te_internal_t *)malloc(sizeof(bier_te_internal_t));
            if (!bier_internal) {
                perror("Malloc bier_internal");
                // TODO: free memory
                return NULL;
            }
            memset(bier_internal, 0, sizeof(bier_te_internal_t));
            bier_internal->bift_id =
                bift_id;  // TODO: this must be more general (include an ID in
                          // the configuration?)
            bier_bift->b[bift_id].bier_te = bier_internal;
            bier_bift->b[bift_id].t = BIER_TE;
            if (fill_bier_internal_bier_te(file, bier_internal) != 0) {
                return NULL;
            }
            printf("Test %d\n", bier_internal->adj_to_bp[0]);
        } else {
            fprintf(stderr, "Unknown BIFT type: %d\n", bift_type);
            return NULL;
        }

        //free(line);
        //line = NULL;
    }

    // Open raw socket to forward the packets
    bier_bift->socket = socket(AF_INET6, SOCK_RAW, 253);
    if (bier_bift->socket < 0) {
        perror("socket BFT");
        free_bier_bft(bier_bift);
        return NULL;
    }

    struct sockaddr_in6 local_router = {
        .sin6_family = AF_INET6,
    };
    char ptr[400];
    memcpy(bier_bift->local.sin6_addr.s6_addr, local_addr.s6_addr,
           sizeof(local_addr.s6_addr));
    memcpy(local_router.sin6_addr.s6_addr, local_addr.s6_addr, sizeof(local_addr.s6_addr));
    fprintf(stderr, "Bind to local address on router:  %s\n",
           inet_ntop(AF_INET6, bier_bift->local.sin6_addr.s6_addr, ptr,
                     sizeof(ptr)));
    if (bind(bier_bift->socket, (struct sockaddr *)&local_router,
             sizeof(local_router)) < 0) {
        perror("Bind local router");
        free_bier_bft(bier_bift);
        return NULL;
    }

    return bier_bift;
}

void update_bitstring(uint64_t *bitstring_ptr, uint64_t *forwarding_bitmask,
                      bitstring_operation op, uint32_t bitstring_max_idx) {
    for (uint32_t i = 0; i < bitstring_max_idx; ++i) {
        uint64_t bitstring = be64toh(bitstring_ptr[i]);
        uint64_t bitmask = forwarding_bitmask[i];
        switch (op) {
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

int send_packet_to_application(uint8_t *payload, size_t payload_length,
                               size_t bier_header_length,
                               bier_application_t *to_app) {
    size_t packet_length = payload_length - bier_header_length;
    uint8_t *packet = &payload[bier_header_length];
    bier_received_packet_t bier_received_packet = {};
    memcpy(&bier_received_packet.ip6_encap_src, &to_app->src.sin6_addr,
           sizeof(to_app->src.sin6_addr));
    bier_received_packet.payload = packet;
    bier_received_packet.payload_length = packet_length;
    bier_received_packet.upstream_router_bfr_id = to_app->src_bfr_id;
    int err = encode_local_bier_payload(to_app->application_socket,
                                        &bier_received_packet,
                                        &to_app->app_addr, to_app->addrlen);
    fprintf(stderr, "DEBUG: combien enoyes: %d\n", err);
}

int bier_non_te_processing(uint8_t *buffer, size_t buffer_length,
                           bier_internal_t *bft, int socket,
                           bier_application_t *to_app) {
    // Remain as general as possible: handle all bitstring length
    uint32_t bitstring_max_idx =
        bft->bitstring_length / 64;                         // In 64 bits words
    uint32_t bitstring_length = bft->bitstring_length / 8;  // In bytes

    // RFC 8279
    uint32_t idx_bfr = 0;
    uint64_t *bitstring_ptr = get_bitstring_ptr(buffer);
    for (int bitstring_idx = bitstring_max_idx - 1; bitstring_idx >= 0;
         --bitstring_idx) {
        if (idx_bfr >= bft->nb_bft_entry) {
            fprintf(stderr,
                    "There seems to be an error. The packet bitstring contains "
                    "a bit set to true that is not mapped to a known BFR in "
                    "the BFT.\n");
            return -1;
        }
        uint64_t bitstring = be64toh(bitstring_ptr[bitstring_idx]);

        // Use modulo operation for non-zero uint64_t words
        uint32_t idx_bfr_word = idx_bfr % 64;
        while ((bitstring >> idx_bfr_word) > 0) {
            if (idx_bfr >= bft->nb_bft_entry) {
                fprintf(stderr,
                        "There seems to be an error. The packet bitstring "
                        "contains a bit set to true that is not mapped to a "
                        "known BFR in the BFT.\n");
                return -1;
            }
            if ((bitstring >> idx_bfr_word) &
                1)  // The current lowest-order bit is set: this BFER must
                    // receive a copy
            {
                // Here we use tje true idx_bfr because we do not use it as
                // index for a table
                if (idx_bfr == bft->local_bfr_id - 1) {
                    fprintf(stderr, "Received a packet for local router %d!\n",
                            bft->local_bfr_id);
                    fprintf(stderr, "Calling local processing function\n");
                    // bier_local_processing->local_processing_function(buffer,
                    // buffer_length, 12 + bitstring_length,
                    // bier_local_processing->args);
                    send_packet_to_application(buffer, buffer_length,
                                               12 + bft->bitstring_length / 8,
                                               to_app);
                    update_bitstring(
                        bitstring_ptr,
                        bft->bft[idx_bfr]->ecmp_entry[0]->forwarding_bitmask,
                        bitwise_u64_and_not, bitstring_max_idx);
                    bitstring = be64toh(bitstring_ptr[bitstring_idx]);
                    ++idx_bfr;
                    idx_bfr_word = idx_bfr % 64;
                    continue;
                }
                fprintf(stderr, "Send a copy to %u (router %u)\n", idx_bfr + 1,
                        bft->local_bfr_id);

                uint8_t packet_copy[buffer_length];  // Room for the IPv6 header
                memset(packet_copy, 0, sizeof(packet_copy));

                uint8_t *bier_header = &packet_copy[0];
                memcpy(bier_header, buffer, buffer_length);

                uint64_t bitstring_copy[bitstring_max_idx];
                memcpy(bitstring_copy, bitstring_ptr,
                       sizeof(uint64_t) * bitstring_max_idx);

                // ECMP may be possible
                int ecmp_entry_idx = 0;
                if (bft->bft[idx_bfr]->nb_ecmp_entries > 1) {
                    printf("Multiple paths for node %u\n", idx_bfr);
                    uint16_t entropy = get_entropy(bier_header);
                    ecmp_entry_idx = entropy % 2;
                    // Choose an ECMP entry
                    // TODO: for now, always choose the last entry but we need
                    // to compute a function of the entropy
                }
                // print_bitstring_message("Bitstring value copy is",
                // bitstring_copy, bitstring_max_idx);
                update_bitstring(bitstring_copy,
                                 bft->bft[idx_bfr]
                                     ->ecmp_entry[ecmp_entry_idx]
                                     ->forwarding_bitmask,
                                 bitwise_u64_and, bitstring_max_idx);
                set_bitstring_ptr(packet_copy, bitstring_copy,
                                  bitstring_max_idx);
                // print_bitstring_message("Bitstring value copy is now",
                // bitstring_copy, bitstring_max_idx);

                // Send copy
                bier_bft_entry_t *bft_entry = bft->bft[idx_bfr];
                char buff[400] = {};
                printf("Should send to %s\n",
                       inet_ntop(AF_INET6,
                                 bft_entry->ecmp_entry[ecmp_entry_idx]
                                     ->bfr_nei_addr.sin6_addr.s6_addr,
                                 buff, sizeof(buff)));
                printf("The bitstirng is %lx\n", bitstring_copy[0]);
                int err = sendto(
                    socket, packet_copy, sizeof(packet_copy), 0,
                    (struct sockaddr *)&bft_entry->ecmp_entry[ecmp_entry_idx]
                        ->bfr_nei_addr,
                    sizeof(
                        bft_entry->ecmp_entry[ecmp_entry_idx]->bfr_nei_addr));
                if (err < 0) {
                    perror("sendto");
                    return -1;
                }
                fprintf(stderr, "Sent packet\n");
                update_bitstring(bitstring_ptr,
                                 bft->bft[idx_bfr]
                                     ->ecmp_entry[ecmp_entry_idx]
                                     ->forwarding_bitmask,
                                 bitwise_u64_and_not, bitstring_max_idx);
                bitstring = be64toh(bitstring_ptr[bitstring_idx]);
            }
            ++idx_bfr;  // Keep track of the index of the BFER to get the
                        // correct entry of the BFT
            idx_bfr_word = idx_bfr % 64;
        }
    }
    fprintf(stderr, "Go out\n");
    return 0;
}

// TODO: inline
bool get_bit_from_bitstring(uint64_t *bitstring, int bit_offset,
                            int bitstring_length) {
    printf("The bitstring is still %lx\n", bitstring[0]);
    printf("Wanting %d, and result is %lx\n", bit_offset,
           be64toh(bitstring[bit_offset / 64]) &
               ((uint64_t)1 << (uint64_t)bit_offset));
    return be64toh(bitstring[bit_offset / 64]) &
           ((uint64_t)1 << (uint64_t)bit_offset);
}

int bier_te_processing(uint8_t *buffer, size_t buffer_length,
                       bier_te_internal_t *bft, int socket,
                       bier_application_t *to_app) {
    uint32_t bitstring_length_in_64 =
        bft->bitstring_length / 64;  // In 64 bits words
    // For BIER-TE the processing is slightly different
    // See https://datatracker.ietf.org/doc/draft-ietf-bier-te-arch/ for more
    // information
    uint64_t *bitstring_ptr = get_bitstring_ptr(buffer);
    // We only iterate over the adjacency BP and the local BFR-BP
    // As we will clear those bits in the packet, we make a local copy
    uint64_t local_bitstring[bitstring_length_in_64];
    memset(local_bitstring, 0, sizeof(local_bitstring));
    // TODO: possible segmentation fault? If the packet respects the bitstring
    // length, should not happen
    memcpy(local_bitstring, bitstring_ptr, sizeof(local_bitstring));
    printf("the bitstring is %lx vs %lx\n", local_bitstring[0],
           bft->global_bitstring[0]);

    // Clear adjacent bits in the packet header to avoid loops
    update_bitstring(bitstring_ptr, bft->global_bitstring, bitwise_u64_and_not,
                     bitstring_length_in_64);
    // Local delivery?
    printf("Du coup apres update: %lx %d %d\n", local_bitstring[0],
           (1 << bft->local_bfr_id),
           get_bit_from_bitstring(local_bitstring, bft->local_bfr_id,
                                  bft->bitstring_length));
    if (get_bit_from_bitstring(local_bitstring, bft->local_bfr_id,
                               bft->bitstring_length)) {
        fprintf(stderr,
                "BIER TE received a packet for local delivery on router %d",
                bft->local_bfr_id);
        // bier_local_processing->local_processing_function(buffer,
        // buffer_length, 12 + bft->bitstring_length / 8,
        // bier_local_processing->args);
        send_packet_to_application(buffer, buffer_length,
                                   12 + bft->bitstring_length / 8, to_app);
    }

    // Iterate over all adjacency BP instead of all bits in the bitstring
    for (int i = 0; i < bft->nb_adjacencies; ++i) {
        int bp_this_adj = bft->adj_to_bp[i];
        printf("Look if must send to bp this adj=%d\n", bp_this_adj);
        if (get_bit_from_bitstring(local_bitstring, bp_this_adj - 1,
                                   bft->bitstring_length)) {
            // Forward to this interface
            // TODO: DNC bit?
            // uint8_t packet_copy[buffer_length];
            // memcpy(packet_copy, buffer, buffer_length);
            // TODO: other things to modify?

            struct sockaddr_in6 nei = bft->bfr_nei_addr[i];
            char buff[400] = {};
            fprintf(
                stderr, "Should send from %d to %s\n", bft->local_bfr_id,
                inet_ntop(AF_INET6, nei.sin6_addr.s6_addr, buff, sizeof(buff)));
            int err = sendto(socket, buffer, buffer_length, 0,
                             (struct sockaddr *)&nei, sizeof(nei));
            if (err < 0) {
                perror("Cannot send packet");
                return -1;
            }
            fprintf(stderr, "Sent packet TE\n");
        }
    }
    return 0;
}

int bier_processing(uint8_t *buffer, size_t buffer_length, bier_bift_t *bier,
                    bier_application_t *to_app) {
    if (buffer_length < 20) {
        return -1;
    }
    // In the packet: 1-indexed, here 0-indexed
    int bift_id = get_bift_id(buffer) - 1;
    //fprintf(stderr, "The given BIFT-ID is %d\n", bift_id);
    //fprintf(stderr, "NB BIFT=%d\n", bier->nb_bift);
    if (bift_id >= bier->nb_bift) {
        fprintf(stderr, "BIFT-ID not supported, error state\n");
        return -1;
    }
    bier_bift_type_t bift = bier->b[bift_id];
    if (bift.t == BIER) {
        fprintf(stderr, "at router %d\n", bift.bier->local_bfr_id);
        return bier_non_te_processing(buffer, buffer_length, bift.bier,
                                      bier->socket, to_app);
    } else if (bift.t == BIER_TE) {
        return bier_te_processing(buffer, buffer_length, bift.bier_te,
                                  bier->socket, to_app);
    } else {
        fprintf(stderr, "Should not happen: %d\n", bift.t);
    }
    return 0;
}
