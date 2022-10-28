#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/un.h>
#include <syslog.h>

#include "bier-sender.h"
#include "include/bier.h"
#include "include/qcbor-encoding.h"

/**
 * @brief BIER daemon. Emulates a BIER forwarding router that receives packets from an IP socket or a UNIX socket.
 * It can be connected to applications using the UNIX socket. The number of applications are limited to 10 for a same BFR.
 * This daemon can also act as a BFIR and a BFER.
 */


/**
 * @brief Free the structure.
 * 
 * @param bier_payload Structure to free.
 */
void free_bier_payload(bier_payload_t *bier_payload) {
    free(bier_payload->bitstring);
    free(bier_payload->payload);
    free(bier_payload);
}

mc_mapping_t *fill_mc_mapping(char *mapping_filename) {
    FILE *file = fopen(mapping_filename, "r");
    if (!file) {
        perror("fill_mc_mapping fopen");
        fprintf(stderr, "The file: %s\n", mapping_filename);
        return NULL;
    }

    ssize_t readed = 0;
    char *line = NULL;
    size_t len = 0;

    // Get the number of entries in the mapping
    int nb_entries = 0;
    while ((readed = getline(&line, &len, file)) != -1) {
        ++nb_entries;
        // free(line)
        // line = NULL;
    }

    if (fseek(file, 0, SEEK_SET) == -1) {
        perror("fill_mc_mapping fseek");
        goto fill_mc_mapping_error;
    }

    mc_mapping_t *mapping = (mc_mapping_t *)malloc(sizeof(mc_mapping_t));
    if (!mapping) {
        perror("fill_mc_mapping malloc");
        goto fill_mc_mapping_error;
    }

    mapping->nb_entries = nb_entries;
    mapping->entries =
        (struct mc_entry *)malloc(sizeof(struct mc_entry) * nb_entries);
    if (!mapping->entries) {
        perror("fill_mc_mapping malloc");
        goto fill_mc_mapping_error_2;
    }
    memset(mapping->entries, 0, sizeof(struct mc_entry) * nb_entries);

    for (int i = 0; i < nb_entries; ++i) {
        if ((readed = getline(&line, &len, file)) == -1) {
            perror("fill_mc_mapping getline");
            goto fill_mc_mapping_error_3;
        }

        char mc_addr[INET6_ADDRSTRLEN];
        char src_addr[INET6_ADDRSTRLEN];
        int bifr_id;

        if (sscanf(line, "%s %s %d", mc_addr, src_addr, &bifr_id) < 0) {
            perror("fill_mc_mapping sscanf");
            goto fill_mc_mapping_error_3;
        }

        // TODO: currently only support for IPv6
        // Parse into IPv6 address
        if (inet_pton(AF_INET6, mc_addr,
                      mapping->entries[i].mc_addr.mc_addr6.s6_addr) != 1) {
            perror("fill_mc_mapping inet_pton");
            goto fill_mc_mapping_error_3;
        }
        mapping->entries[i].family = AF_INET6;

        if (inet_pton(AF_INET6, src_addr,
                      mapping->entries[i].src_addr.src_addr6.s6_addr) != 1) {
            perror("fill_mc_mapping inet_pton src mc");
            goto fill_mc_mapping_error_3;
        }

        mapping->entries[i].bifr_id = bifr_id;

        // free(line);
        // line = NULL;
    }

    fclose(file);
    return mapping;

fill_mc_mapping_error_3:
    free(mapping->entries);
fill_mc_mapping_error_2:
    free(mapping);
fill_mc_mapping_error:
    fclose(file);
    return NULL;
}

void usage(char *prog_name) {
    fprintf(stderr, "USAGE:\n");
    fprintf(stderr, "    %s [OPTIONS] -c <> -b <> -a <> -m <> -g <>\n", prog_name);
    fprintf(stderr,
            "    -c config file: static BIFT configuration file path\n");
    fprintf(stderr,
            "    -b bier socket addr: path to the UNIX socket path of the BIER daemon\n");
    fprintf(stderr,
            "    -a application socket path: [DECREPATED] path to the UNIX socket of the application that uses the daemon\n");
    fprintf(stderr,
            "    -m mapping path: mapping from IP address to BFR-id\n");
    fprintf(stderr, "    -g group path: path to the file containing the multicast groups and the corresponding source BFR-ids\n");
    fprintf(stderr, "    -i: use IPv4 instead of IPv6 if added\n");
}

typedef struct {
    char config_file[NAME_MAX];
    char bier_socket_path[NAME_MAX];
    char application_socket_path[NAME_MAX];
    char ip_2_id_mapping[NAME_MAX];
    char mc_group_mapping[NAME_MAX];
    bool use_ipv4;
} args_t;

void parse_args(args_t *args, int argc, char *argv[]) {
    memset(args, 0, sizeof(args_t));
    int opt;
    bool has_config_file, has_bier_socket_path, has_application_socket_path,
        has_ip_2_id_mapping, has_mc_group_mapping;
    args->use_ipv4 = false;

    while ((opt = getopt(argc, argv, "c:b:a:m:g:i")) != -1) {
        switch (opt) {
            case 'c': {
                strcpy(args->config_file, optarg);
                has_config_file = true;
                break;
            }
            case 'b': {
                strcpy(args->bier_socket_path, optarg);
                has_bier_socket_path = true;
                break;
            }
            case 'a': {
                strcpy(args->application_socket_path, optarg);
                has_application_socket_path = true;
                break;
            }
            case 'm': {
                strcpy(args->ip_2_id_mapping, optarg);
                has_ip_2_id_mapping = true;
                break;
            }
            case 'g': {
                strcpy(args->mc_group_mapping, optarg);
                has_mc_group_mapping = true;
                break;
            }
            case 'i': {
                args->use_ipv4 = true;
                break;
            }
            default: {
                usage(argv[0]);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (!(has_config_file && has_bier_socket_path &&
          has_application_socket_path && has_ip_2_id_mapping)) {
        fprintf(stderr,
                "Missing arguments: config? %u, bier? %u, application? %u, "
                "mapping? %u\n",
                has_config_file, has_bier_socket_path,
                has_application_socket_path, has_ip_2_id_mapping);
        exit(EXIT_FAILURE);
    }
}

int64_t get_id_from_address(sockaddr_uniform_t *addr, bier_addr2bifr_t *mapping,
                            bool use_ipv4) {
    for (int i = 0; i < mapping->nb_entries; ++i) {
        // fprintf(stderr, "Comparing %x %x\n", mapping->addrs[i].s6_addr[3],
        // addr->s6_addr[3]);
        int res;
        if (use_ipv4) {
            res = memcmp(&mapping->addrs[i].v4.s_addr,
                         &addr->v4.sin_addr.s_addr, sizeof(uint32_t));
        } else {
            res =
                memcmp(mapping->addrs[i].v6.s6_addr, addr->v6.sin6_addr.s6_addr,
                       sizeof(addr->v6.sin6_addr.s6_addr));
        }
        if (res == 0) {
            return mapping->bfr_ids[i];
        }
    }
    return -1;
}

bier_addr2bifr_t *read_addr_mapping(char *filename, bool use_ipv4) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Filename: %s\n", filename);
        perror("read_addr_mapping");
        return NULL;
    }

    bier_addr2bifr_t *mapping =
        (bier_addr2bifr_t *)malloc(sizeof(bier_addr2bifr_t));
    if (!mapping) {
        perror("read_addr_mapping malloc");
        return NULL;
    }
    memset(mapping, 0, sizeof(bier_addr2bifr_t));

    ssize_t readed = 0;
    char *line = NULL;
    size_t len = 0;

    // Count the number of lines
    int nb_entries = 0;
    while ((readed = getline(&line, &len, file)) != -1) {
        ++nb_entries;
        // free(line);
        // line = NULL;
    }

    if (fseek(file, 0, SEEK_SET) == -1) {
        perror("fseek");
        return NULL;
    }

    mapping->nb_entries = nb_entries;
    mapping->addrs =
        (in_addr_common_t *)malloc(sizeof(in_addr_common_t) * nb_entries);
    if (!mapping->addrs) {
        perror("read_addr_mapping_atoi malloc2");
        return NULL;
    }
    mapping->bfr_ids = (uint64_t *)malloc(sizeof(uint64_t) * nb_entries);
    if (!mapping->bfr_ids) {
        perror("read_addr_mapping_atoi malloc3");
        return NULL;
    }
    memset(mapping->addrs, 0, sizeof(in_addr_common_t) * nb_entries);
    memset(mapping->bfr_ids, 0, sizeof(uint64_t) * nb_entries);

    for (int i = 0; i < nb_entries; ++i) {
        if ((readed = getline(&line, &len, file)) == -1) {
            perror("read_addr_mapping getline3");
            return NULL;
        }
        char addr[100];
        uint64_t id;
        uint32_t prlength;

        if (sscanf(line, "%lu %[^/]/%d\n", &id, addr, &prlength) < 0) {
            perror("sscanf");
            return NULL;
        }

        // Parse into IPv6 address
        int err;
        if (use_ipv4) {
            err = inet_pton(AF_INET, addr, &mapping->addrs[i].v4.s_addr);
        } else {
            err = inet_pton(AF_INET6, addr, &mapping->addrs[i].v6.s6_addr);
        }
        if (err != 1) {
            perror("inet_pton");
            fprintf(stderr, "Cannot convert to address: %s\n", addr);
            fprintf(stderr, "Comprends aps %s - %u\n", addr, prlength);
            return NULL;
        }

        mapping->bfr_ids[i] = id;

        // free(line);
        // line = NULL;
    }

    fclose(file);

    return mapping;
}

int process_unix_message_is_payload(void *bier_payload_void, bier_bift_t *bier,
                                    bier_all_apps_t *all_apps, bool use_ipv4) {
    bier_payload_t *bier_payload = (bier_payload_t *)bier_payload_void;
    fprintf(stderr, "BIER payload of %lu bytes\n",
            bier_payload->payload_length);
    fprintf(stderr, "BIER bitstring: ");
    for (int i = 0; i < bier_payload->bitstring_length; ++i) {
        fprintf(stderr, "%x ", bier_payload->bitstring[i]);
    }
    fprintf(stderr, "\n");
    // TODO: proto must be sent also, and BIFT-ID based on TE?
    bier_header_t *bh =
        init_bier_header((const uint64_t *)bier_payload->bitstring,
                         bier_payload->bitstring_length * 8,
                         bier_payload->proto, bier_payload->use_bier_te);
    // TODO: check error
    my_packet_t *packet = encap_bier_packet(bh, bier_payload->payload_length,
                                            bier_payload->payload);
    memset(&all_apps->src, 0, sizeof(all_apps->src));
    int err = bier_processing(packet->packet, packet->packet_length, bier,
                              all_apps, use_ipv4);
    if (err < 0) {
        fprintf(stderr,
                "Error when processing the BIER packet at the "
                "router... exiting...\n");
        my_packet_free(packet);  // TODO: the frees are not logical here
        return -1;
    }
    free_bier_payload(bier_payload);
    release_bier_header(bh);
    my_packet_free(packet);
    return 0;
}

int get_bifr_id_from_mc_addr(int family, uint8_t *mc_addr,
                             mc_mapping_t *mapping) {
    // TODO: currently only support for IPv6
    if (family != AF_INET6) {
        fprintf(stderr, "Currently only supports IPv6");
        return -1;
    }

    for (int i = 0; i < mapping->nb_entries; ++i) {
        if (memcmp(mapping->entries[i].mc_addr.mc_addr6.s6_addr, mc_addr,
                   sizeof(mapping->entries[i].mc_addr.mc_addr6.s6_addr)) == 0) {
            return i;
        }
    }

    fprintf(stderr, "Did not found the BIFR ID of the address: ");
    for (int i = 0; i < 16; ++i) {
        fprintf(stderr, "%x ", mc_addr[i]);
    }
    fprintf(stderr, "\n");
    return -1;
}

int send_multicast_join_or_leave(bier_bind_t *bind, mc_mapping_t *mapping, bier_bift_t *bier, bier_all_apps_t *all_apps, int idx_map, bool use_ipv4, bool is_join) {
    bier_application_t *app = &all_apps->apps[idx_map];
    
    // TODO: currently only support IPv6 multicast destination
    if (app->mc_addr_family != AF_INET6) {
        fprintf(stderr,
                "Does not support more than IPv6 destination address\n");
        return -1;
    }

    // TODO: the bitstring should be safer, in case we have a very long
    // bitstring
    int idx_mapping = get_bifr_id_from_mc_addr(
        AF_INET6, bind->mc_sockaddr.v6.sin6_addr.s6_addr, mapping);
    if (idx_mapping < 0) {
        return -1;
    }
    fprintf(stderr, "P2\n");
    int bfir_id = mapping->entries[idx_mapping].bifr_id;
    uint64_t bitstring = 1 << (bfir_id - 1);  // 0-indexed
    // The local BFER updated its internal database
    // Send a packet to the BIFR of the multicast group
    // to notify an update in the bitstring
    // TODO: currently the packet is sent with the multicast address
    // as destination it MUST be the address of the multicast source
    // instead.
    // TODO: this should follow the IETF draft, especially the number of
    // repetitions of a packet
    fprintf(stderr, "P3\n");
    struct in6_addr src, dst;
    memcpy(src.s6_addr, bier->local.v6.sin6_addr.s6_addr,
            sizeof(src.s6_addr));
    memcpy(dst.s6_addr,
            mapping->entries[idx_mapping].src_addr.src_addr6.s6_addr,
            sizeof(dst.s6_addr));

    // TODO: currently the packet is really dumb. It just contain a byte to
    // tell the BFIR if the current BFER joins or leaves the multicast
    // group, And the BFR-ID of the current BFER
    int32_t payload[2];
    payload[0] = is_join ? 1 : 2;
    // TODO: not sure about this line
    payload[1] = bier->b->bier->local_bfr_id;  // Local BFR-ID

    bier_header_t *bh = init_bier_header(&bitstring, 64, BIERPROTO_IPV6, 1);
    if (!bh) {
        return -1;
    }
    fprintf(stderr, "P4\n");

    my_packet_t *packet = create_bier_ipv6_from_payload(
        bh, &src, &dst, sizeof(payload), (uint8_t *)payload);
    if (!packet) {
        release_bier_header(bh);
        return -1;
    }

    fprintf(stderr,
            "Will send a packet to the BFIR %d to set the bit in the "
            "bitstring. Bitstring is: %lx\n",
            bfir_id, bitstring);
    int err = bier_processing(packet->packet, packet->packet_length, bier,
                                all_apps, use_ipv4);
    if (err < 0) {
        fprintf(stderr, "Error when sending the BIER BINDING\n");
        return -1;
    }

    my_packet_free(packet);
    release_bier_header(bh);
}

int process_unix_message_is_bind_join(bier_bind_t *bind, bier_all_apps_t *all_apps,
                                      bier_bift_t *bier, mc_mapping_t *mapping,
                                      bool use_ipv4) {
    fprintf(stderr, "Message is a bind JOIN\n");
    if (all_apps->nb_apps >= BIER_MAX_APPS) {
        fprintf(stderr, "Cannot add another application to BIER");
        return -1;
    }
    fprintf(stderr,
            "Received a bind message for proto %d and UNIX path %s. Is "
            "listener? %u JOIN ? %u\n",
            bind->proto, bind->unix_path, bind->is_listener, bind->is_join);
    // struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&bind->mc_sockaddr;
    // fprintf(stderr, "Bound to: ");
    // for (int j = 0; j < 16; ++j) {
    //     fprintf(stderr, "%x ", addr->sin6_addr.s6_addr[j]);
    // }
    fprintf(stderr, "\n");
    // Search for empty space in the array
    int idx_map;
    for (idx_map = 0; idx_map < BIER_MAX_APPS; ++idx_map) {
        if (!all_apps->apps[idx_map].is_active) {
            break; // Found an empty space
        }
    }
    if (idx_map == BIER_MAX_APPS) {
        fprintf(stderr, "Should not happen: more apps than stated\n");
        return -1;
    }
    bier_application_t *app = &all_apps->apps[idx_map];
    memset(app, 0, sizeof(bier_application_t));
    app->proto = bind->proto;
    app->app_addr.sun_family = AF_UNIX;
    strcpy(app->app_addr.sun_path, bind->unix_path);
    app->addrlen = sizeof(struct sockaddr_un);
    app->is_listener = bind->is_listener;
    app->is_active = true;

    // Sanity check
    if (bind->mc_sockaddr.v6.sin6_family != AF_INET6 &&
        bind->mc_sockaddr.v4.sin_family != AF_INET) {
        fprintf(stderr, "Does not support other family than IPv6 and IPV4\n");
        return -1;
    }

    // TODO: note that this could be a source of bug
    bool mc_dst_is_ipv4 = bind->mc_sockaddr.v6.sin6_family == AF_INET;

    fprintf(stderr, "P1\n");
    if (mc_dst_is_ipv4) {
        app->mc_addr_family = AF_INET;
        memcpy(&app->mc_addr.mc_ipv4.s_addr,
               &bind->mc_sockaddr.v4.sin_addr.s_addr,
               sizeof(bind->mc_sockaddr.v4.sin_addr.s_addr));
    } else {
        app->mc_addr_family = AF_INET6;
        memcpy(app->mc_addr.mc_ipv6.s6_addr,
               &bind->mc_sockaddr.v6.sin6_addr.s6_addr,
               sizeof(bind->mc_sockaddr.v6.sin6_addr.s6_addr));
    }
    fprintf(stderr, "P1,5\n");

    if (bind->is_listener) {
        if (send_multicast_join_or_leave(bind, mapping, bier, all_apps, idx_map, use_ipv4, true) < 0) {
            return -1;
        }
    }
    free(bind);
    ++all_apps->nb_apps;
    return 0;
}

int process_unix_message_is_bind_leave(bier_bind_t *bind, bier_all_apps_t *all_apps,
                                      bier_bift_t *bier, mc_mapping_t *mapping,
                                      bool use_ipv4) {
    fprintf(stderr, "Message is a bind LEAVE\n");
    // Simply set the address as not active once we find it
    int idx = -1;
    for (int i = 0; i < BIER_MAX_APPS; ++i) {
        if (!all_apps->apps[i].is_active) {
            continue;
        }
        if (all_apps->apps[i].proto == bind->proto && all_apps->apps[i].is_listener == bind->is_listener) {
            // Also verify the IPv6 multicast address
            int addr_res;
            if (use_ipv4) {
                addr_res = memcmp(&all_apps->apps[i].mc_addr.mc_ipv4.s_addr, &bind->mc_sockaddr.v4.sin_addr.s_addr, sizeof(bind->mc_sockaddr.v4.sin_addr.s_addr));
            } else {
                addr_res = memcmp(all_apps->apps[i].mc_addr.mc_ipv6.s6_addr, bind->mc_sockaddr.v6.sin6_addr.s6_addr, sizeof(bind->mc_sockaddr.v6.sin6_addr.s6_addr));
            }
            if (addr_res == 0) {
                idx = i;
                break;
            }
        }
    }
    if (idx == -1) {
        fprintf(stderr, "Cannot find the right group in bind LEAVE\n");
        return -1;
    }

    // With the index, we just need to set the address as inactive
    all_apps->apps[idx].is_active = false;

    if (all_apps->apps[idx].is_listener) {
        if (send_multicast_join_or_leave(bind, mapping, bier, all_apps, idx, use_ipv4, false) < 0) {
            return -1;
        }
    }

    return 0;

}

int process_unix_message_is_bind(void *message, bier_all_apps_t *all_apps,
                                 bier_bift_t *bier, mc_mapping_t *mapping,
                                 bool use_ipv4) {
    bier_bind_t *bind = (bier_bind_t *)message;
    if (bind->is_join) {
        process_unix_message_is_bind_join(bind, all_apps, bier, mapping, use_ipv4);
    } else {
        process_unix_message_is_bind_leave(bind, all_apps, bier, mapping, use_ipv4);
    }
}

int main(int argc, char *argv[]) {
    // Enable logs by default.
    openlog(NULL, LOG_DEBUG | LOG_PID | LOG_PERROR, LOG_USER);

    args_t args;
    parse_args(&args, argc, argv);

    bier_bift_t *bier = read_config_file(args.config_file, args.use_ipv4);
    if (!bier) {
        exit(EXIT_FAILURE);
    }

    bier_addr2bifr_t *mapping =
        read_addr_mapping(args.ip_2_id_mapping, args.use_ipv4);
    if (!mapping) {
        exit(1);
    }

    // This socket receives packets from the Application and sends them in the
    // BIER network
    int listening_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (listening_socket == -1) {
        perror("Listening UNIX socket");
        exit(EXIT_FAILURE);
    }

    bier_all_apps_t *all_apps =
        (bier_all_apps_t *)malloc(sizeof(bier_all_apps_t));
    if (!all_apps) {
        perror("malloc all apps");
        exit(EXIT_FAILURE);
    }
    memset(all_apps, 0, sizeof(bier_all_apps_t));
    all_apps->application_socket = listening_socket;

    mc_mapping_t *mc2id_mapping = fill_mc_mapping(args.mc_group_mapping);
    if (!mapping) {
        exit(EXIT_FAILURE);
    }

    // This socket is used to forward packets from the BIER network to the
    // application
    int sending_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sending_socket == -1) {
        perror("Sending UNIX socket");
        close(listening_socket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un unix_local = {};
    unix_local.sun_family = AF_UNIX;
    strcpy(unix_local.sun_path, args.bier_socket_path);
    int data_len = strlen(unix_local.sun_path) + sizeof(unix_local.sun_family);

    // https://medium.com/swlh/getting-started-with-unix-domain-sockets-4472c0db4eb1
    if (remove(args.bier_socket_path) == -1 && errno != ENOENT) {
        perror("Remove unix socket path");
        close(sending_socket);
        close(listening_socket);
        exit(EXIT_FAILURE);
    }

    if (bind(sending_socket, (struct sockaddr *)&unix_local,
             sizeof(struct sockaddr_un)) == -1) {
        perror("Bind unix socket");
        close(sending_socket);
        close(listening_socket);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Bound to UNIX socket to listen to application packets!\n");

    // Allocate poll fds
    int nfds = 2;
    struct pollfd *pfds = (struct pollfd *)calloc(nfds, sizeof(struct pollfd));
    if (!pfds) {
        perror("Calloc pfds");
        close(sending_socket);
        close(listening_socket);
        exit(EXIT_FAILURE);
    }

    pfds[0].fd = bier->socket;
    pfds[1].fd = sending_socket;

    pfds[0].events = POLLIN;
    pfds[1].events = POLLIN;

    // BIER socket buffer
    uint16_t buffer_size = 1500;
    uint8_t *buffer = (uint8_t *)malloc(sizeof(uint8_t) * buffer_size);

    sockaddr_uniform_t remote = {};
    socklen_t remote_len;
    if (args.use_ipv4) {
        remote_len = sizeof(struct sockaddr_in);
    } else {
        remote_len = sizeof(struct sockaddr_in6);
    }

    // UNIX socket buffer
    size_t unix_buffer_size = sizeof(uint8_t) * (4096);
    uint8_t *unix_buffer = (uint8_t *)malloc(unix_buffer_size);
    if (!unix_buffer) {
        perror("malloc unix buffer");
    }
    memset(unix_buffer, 0, unix_buffer_size);

    while (1) {
        fprintf(stderr, "About to poll...\n");
        int ready = poll(pfds, nfds, -1);
        if (ready == -1) {
            perror("Poll");
            break;
        }

        fprintf(stderr, "Ready: %d\n", ready);
        for (int i = 0; i < nfds; ++i) {
            if (pfds[i].revents & POLLIN) {
                fprintf(stderr, "Got a message from %d!\n", i);
                if (i == 1) {
                    fprintf(stderr, "UNIX socket\n");
                    // TODO:
                    ssize_t nb_read =
                        recv(pfds[i].fd, unix_buffer, unix_buffer_size, 0);
                    if (nb_read < 0) {
                        perror("read");
                        break;
                    }
                    fprintf(stderr, "Received a message of length: %lu\n",
                            nb_read);

                    bier_message_type type;
                    void *decoded_message =
                        decode_application_message(unix_buffer, nb_read, &type);
                    if (!decoded_message) {
                        fprintf(stderr, "Confirmed\n");
                        break;
                    }

                    switch (type) {
                        case PACKET: {
                            if (process_unix_message_is_payload(
                                    decoded_message, bier, all_apps,
                                    args.use_ipv4) < 0) {
                                goto error;
                            }
                            break;
                        }
                        case BIND: {
                            if (process_unix_message_is_bind(
                                    decoded_message, all_apps, bier,
                                    mc2id_mapping, args.use_ipv4) < 0) {
                                goto error;
                            }
                            break;
                        }
                        default: {
                            fprintf(stderr, "confirmed");
                            goto error;
                        }
                    }
                } else {
                    fprintf(stderr, "BIER socket\n");
                    memset(buffer, 0, sizeof(uint8_t) * buffer_size);
                    char buff[100];
                    size_t length = recvfrom(
                        pfds[i].fd, buffer, sizeof(uint8_t) * buffer_size, 0,
                        (struct sockaddr *)&remote, &remote_len);
                    const void *err;
                    if (args.use_ipv4) {
                        err = inet_ntop(AF_INET, &remote.v4.sin_addr.s_addr,
                                        buff, sizeof(buff));
                    } else {
                        err = inet_ntop(AF_INET6, remote.v6.sin6_addr.s6_addr,
                                        buff, sizeof(buff));
                    }
                    if (!err) {
                        fprintf(stderr,
                                "Cannot convert the received address to human "
                                "form\n");
                        perror("inet_ntop bfr");
                        break;
                    }

                    memcpy(&all_apps->src, &remote, sizeof(sockaddr_uniform_t));
                    all_apps->src_bfr_id =
                        get_id_from_address(&remote, mapping, args.use_ipv4);
                    // fprintf(stderr, "THE ID is %d\n", to_app.src_bfr_id);
                    // With IPv4 it seems that we also get the IPv4 header
                    if (args.use_ipv4) {
                        bier_processing(&buffer[20], length - 20, bier,
                                        all_apps, args.use_ipv4);
                    } else {
                        bier_processing(buffer, length, bier, all_apps,
                                        args.use_ipv4);
                    }

                } 
            } else if (pfds[i].revents != 0) {
                printf("  fd=%d; events: %s%s%s\n", pfds[i].fd,
                       (pfds[i].revents & POLLIN) ? "POLLIN " : "",
                       (pfds[i].revents & POLLHUP) ? "POLLHUP " : "",
                       (pfds[i].revents & POLLERR) ? "POLLERR " : "");
            }
        }
    }

error:
    free(buffer);
    free(unix_buffer);
    fprintf(stderr, "Closing the program on router\n");
    free_bier_bft(bier);
    free(mc2id_mapping->entries);
    free(mc2id_mapping);
    close(sending_socket);
    close(listening_socket);
}
