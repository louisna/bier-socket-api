#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <getopt.h>

#include "bier-sender.h"
#include "include/bier.h"
#include "include/qcbor-encoding.h"

void print_buffer(uint8_t *buffer, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (buffer[i] < 16) {
            fprintf(stderr, "0");
        }
        fprintf(stderr, "%x ", buffer[i]);
    }
    fprintf(stderr, "\n");
}

void free_bier_payload(bier_payload_t *bier_payload) {
    free(bier_payload->bitstring);
    free(bier_payload->payload);
    free(bier_payload);
}

typedef struct {
    char config_file[NAME_MAX];
    char bier_socket_path[NAME_MAX];
    char application_socket_path[NAME_MAX];
    char ip_2_id_mapping[NAME_MAX];
    bool use_ipv4;
} args_t;

void parse_args(args_t *args, int argc, char *argv[]) {
    memset(args, 0, sizeof(args_t));
    int opt;
    bool has_config_file, has_bier_socket_path, has_application_socket_path, has_ip_2_id_mapping;
    args->use_ipv4 = false;

    while ((opt = getopt(argc, argv, "c:b:a:m:i")) != -1) {
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
            case 'i': {
                args->use_ipv4 = true;
                break;
            }
            default: {
                fprintf(stderr, "TODO default usage\n");
                break;
            }
        }
    }

    if (!(has_config_file && has_bier_socket_path && has_application_socket_path && has_ip_2_id_mapping)) {
        fprintf(stderr, "Missing arguments: config? %u, bier? %u, application? %u, mapping? %u\n", has_config_file, has_bier_socket_path, has_application_socket_path, has_ip_2_id_mapping);
        exit(EXIT_FAILURE);
    }
}

int64_t get_id_from_address(sockaddr_uniform_t *addr, bier_addr2bifr_t *mapping, bool use_ipv4) {
    for (int i = 0; i < mapping->nb_entries; ++i) {
        // fprintf(stderr, "Comparing %x %x\n", mapping->addrs[i].s6_addr[3], addr->s6_addr[3]);
        int res;
        if (use_ipv4) {
            res = memcmp(&mapping->addrs[i].v4.s_addr, &addr->v4.sin_addr.s_addr, sizeof(uint32_t));
        } else {
            res = memcmp(mapping->addrs[i].v6.s6_addr, addr->v6.sin6_addr.s6_addr, sizeof(addr->v6.sin6_addr.s6_addr));
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
        perror("read_addr_mapping");
        return NULL;
    }

    bier_addr2bifr_t *mapping = (bier_addr2bifr_t *)malloc(sizeof(bier_addr2bifr_t));
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
        free(line);
        line = NULL;
    }

    if (fseek(file, 0, SEEK_SET) == -1) {
        perror("fseek");
        return NULL;
    }

    mapping->nb_entries = nb_entries;
    mapping->addrs = (in_addr_common_t *)malloc(sizeof(in_addr_common_t) * nb_entries);
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

        free(line);
        line = NULL;
    }

    return mapping;
}

int main(int argc, char *argv[]) {
    args_t args;
    parse_args(&args, argc, argv);

    bier_bift_t *bier = read_config_file(args.config_file, args.use_ipv4);
    if (!bier) {
        exit(EXIT_FAILURE);
    }

    bier_addr2bifr_t *mapping = read_addr_mapping(args.ip_2_id_mapping, args.use_ipv4);
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

    // Destination is the application waiting for BIER packets
    struct sockaddr_un app_addr = {};
    app_addr.sun_family = AF_UNIX;
    strcpy(app_addr.sun_path, args.application_socket_path);

    bier_application_t to_app = {};
    to_app.application_socket = listening_socket;
    memcpy(&to_app.app_addr, &app_addr, sizeof(struct sockaddr_un));
    to_app.addrlen = sizeof(struct sockaddr_un);

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
                if (i == 0) {
                    fprintf(stderr, "BIER socket\n");
                    memset(buffer, 0, sizeof(uint8_t) * buffer_size);
                    char buff[100];
                    size_t length = recvfrom(
                        pfds[i].fd, buffer, sizeof(uint8_t) * buffer_size, 0,
                        (struct sockaddr *)&remote, &remote_len);
                    const void *err;
                    if (args.use_ipv4) {
                        err = inet_ntop(AF_INET, &remote.v4.sin_addr.s_addr, buff, sizeof(buff));
                    } else {
                        err = inet_ntop(AF_INET6, remote.v6.sin6_addr.s6_addr, buff, sizeof(buff));
                    }
                    if (!err) {
                        fprintf(stderr, "Cannot convert the received address to human form\n");
                        perror("inet_ntop bfr");
                        break;
                    }
                    fprintf(stderr, "Going to print the buffer\n");
                    print_buffer(buffer, length);
                    // inet_ntop(AF_INET6, &remote, buff, sizeof(remote));
                    fprintf(stderr, "src %s\n", buff);
                    memcpy(&to_app.src, &remote, sizeof(sockaddr_uniform_t));

                    to_app.src_bfr_id = get_id_from_address(&remote, mapping, args.use_ipv4);
                    // fprintf(stderr, "THE ID is %d\n", to_app.src_bfr_id);
                    // With IPv4 it seems that we also get the IPv4 header
                    if (args.use_ipv4) {
                        bier_processing(&buffer[20], length - 20, bier, &to_app, args.use_ipv4);
                    } else {
                        bier_processing(buffer, length, bier, &to_app, args.use_ipv4);
                    }
                } else {
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

                    // Convert to recover the BIER packet
                    bier_payload_t *bier_payload =
                        (bier_payload_t *)malloc(sizeof(bier_payload_t));
                    if (!bier_payload) {
                        perror("malloc bier payload");
                        break;
                    }
                    UsefulBufC cbor = {unix_buffer, nb_read};
                    QCBORError uErr = decode_bier_payload(cbor, bier_payload);
                    // TODO: check error

                    fprintf(stderr, "BIER payload of %lu bytes\n",
                            bier_payload->payload_length);
                    fprintf(stderr, "BIER bitstring: ");
                    for (int i = 0; i < bier_payload->bitstring_length; ++i) {
                        fprintf(stderr, "%x ", bier_payload->bitstring[i]);
                    }
                    fprintf(stderr, "\n");
                    // TODO: proto must be sent also, and BIFT-ID based on TE?
                    bier_header_t *bh = init_bier_header(
                        (const uint64_t *)bier_payload->bitstring,
                        bier_payload->bitstring_length * 8, 6,
                        bier_payload->use_bier_te);
                    // TODO: check error
                    my_packet_t *packet =
                        encap_bier_packet(bh, bier_payload->payload_length,
                                          bier_payload->payload);
                    memset(&to_app.src, 0, sizeof(to_app.src));
                    int err = bier_processing(
                        packet->packet, packet->packet_length, bier, &to_app, args.use_ipv4);
                    if (err < 0) {
                        fprintf(stderr,
                                "Error when processing the BIER packet at the "
                                "router... exiting...\n");
                        my_packet_free(packet);
                        break;
                    }
                    free_bier_payload(bier_payload);
                    release_bier_header(bh);
                    my_packet_free(packet);
                }
            } else if (pfds[i].revents != 0) {
                printf("  fd=%d; events: %s%s%s\n", pfds[i].fd,
                       (pfds[i].revents & POLLIN) ? "POLLIN " : "",
                       (pfds[i].revents & POLLHUP) ? "POLLHUP " : "",
                       (pfds[i].revents & POLLERR) ? "POLLERR " : "");
            }
        }
    }

    free(buffer);
    free(unix_buffer);
    fprintf(stderr, "Closing the program on router\n");
    free_bier_bft(bier);
    close(sending_socket);
    close(listening_socket);
}
