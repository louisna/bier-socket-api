#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/un.h>

#include "bier-sender.h"
#include "include/bier.h"
#include "include/qcbor-encoding.h"

void print_buffer(uint8_t *buffer, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (buffer[i] < 16) {
            printf("0");
        }
        printf("%x ", buffer[i]);
    }
    printf("\n");
}

void free_bier_payload(bier_payload_t *bier_payload) {
    free(bier_payload->bitstring);
    free(bier_payload->payload);
    free(bier_payload);
}

int64_t get_id_from_address(struct in6_addr *addr, bier_addr2bifr_t *mapping) {
    for (int i = 0; i < mapping->nb_entries; ++i) {
        // fprintf(stderr, "Comparing %x %x\n", mapping->addrs[i].s6_addr[3],
        // addr->s6_addr[3]);
        if (memcmp(mapping->addrs[i].s6_addr, addr->s6_addr,
                   sizeof(addr->s6_addr)) == 0) {
            return mapping->bfr_ids[i];
        }
    }
    return -1;
}

bier_addr2bifr_t *read_addr_mapping(char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "FIlename: %s\n", filename);
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
        free(line);
        line = NULL;
    }

    if (fseek(file, 0, SEEK_SET) == -1) {
        perror("fseek");
        return NULL;
    }

    mapping->nb_entries = nb_entries;
    mapping->addrs =
        (struct in6_addr *)malloc(sizeof(struct in6_addr) * nb_entries);
    if (!mapping->addrs) {
        perror("read_addr_mapping_atoi malloc2");
        return NULL;
    }
    mapping->bfr_ids = (uint64_t *)malloc(sizeof(uint64_t) * nb_entries);
    if (!mapping->bfr_ids) {
        perror("read_addr_mapping_atoi malloc3");
        return NULL;
    }
    memset(mapping->addrs, 0, sizeof(struct in6_addr) * nb_entries);
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
        if (inet_pton(AF_INET6, addr, mapping->addrs[i].s6_addr) != 1) {
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

int process_unix_message_is_payload(void *bier_payload_void,
                                    bier_bift_t *bier,
                                    bier_all_apps_t *all_apps) {
    bier_payload_t *bier_payload = (bier_payload_t *)bier_payload_void;
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
        bier_payload->bitstring_length * 8, bier_payload->proto, bier_payload->use_bier_te);
    // TODO: check error
    my_packet_t *packet = encap_bier_packet(bh, bier_payload->payload_length,
                                            bier_payload->payload);
    memset(&all_apps->src, 0, sizeof(all_apps->src));
    int err =
        bier_processing(packet->packet, packet->packet_length, bier, all_apps);
    if (err < 0) {
        fprintf(stderr,
                "Error when processing the BIER packet at the "
                "router... exiting...\n");
        my_packet_free(packet); // TODO: the frees are not logical here
        return -1;
    }
    free_bier_payload(bier_payload);
    release_bier_header(bh);
    my_packet_free(packet);
    return 0;
}

int process_unix_message_is_bind(void *message, bier_all_apps_t *all_apps) {
    if (all_apps->nb_apps >= BIER_MAX_APPS) {
        fprintf(stderr, "Cannot add another application to BIER");
        return -1;
    }
    
    bier_bind_t *bind = (bier_bind_t *)message;
    fprintf(stderr, "Received a bind message for proto %d and UNIX path %s\n", bind->proto, bind->unix_path);
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&bind->mc_sockaddr;
    fprintf(stderr, "Bound to: ");
    for (int j = 0; j < 16; ++j) {
        fprintf(stderr, "%x ", addr->sin6_addr.s6_addr[j]);
    }
    fprintf(stderr, "\n");
    bier_application_t *app = &all_apps->apps[all_apps->nb_apps];
    memset(app, 0, sizeof(bier_application_t));
    app->proto = bind->proto;
    app->app_addr.sun_family = AF_UNIX;
    strcpy(app->app_addr.sun_path, bind->unix_path);
    app->addrlen = sizeof(struct sockaddr_un);

    // Also add the IPv6 multicast address
    // TODO: currently only support for IPv6
    if (bind->mc_sockaddr.sin6_family != AF_INET6) {
        fprintf(stderr, "Does not support other family than IPv6\n");
        return -1;
    } 
    memcpy(&app->mc_sockaddr, &bind->mc_sockaddr, sizeof(struct sockaddr_in6));

    free(bind);
    ++all_apps->nb_apps;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(
            stderr,
            "Usage: %s <config_file> <send UNIX socket> <listen UNIX socket> <ipv6-2-id-mapping>\n",
            argv[0]);
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];
    bier_bift_t *bier = read_config_file(filename);
    if (!bier) {
        exit(EXIT_FAILURE);
    }

    const char *sending_socket_path = argv[2];
    const char *listening_socket_path = argv[3];

    bier_addr2bifr_t *mapping = read_addr_mapping(argv[4]);
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

    bier_all_apps_t *all_apps = (bier_all_apps_t *)malloc(sizeof(bier_all_apps_t));
    if (!all_apps) {
        perror("malloc all apps");
        exit(EXIT_FAILURE);
    }
    memset(all_apps, 0, sizeof(bier_all_apps_t));
    all_apps->application_socket = listening_socket;

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
    strcpy(unix_local.sun_path, sending_socket_path);
    int data_len = strlen(unix_local.sun_path) + sizeof(unix_local.sun_family);

    // https://medium.com/swlh/getting-started-with-unix-domain-sockets-4472c0db4eb1
    if (remove(sending_socket_path) == -1 && errno != ENOENT) {
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
    struct sockaddr_in6 remote = {};
    socklen_t remote_len = sizeof(remote);

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
                    fprintf(
                        stderr,
                        "Received packet of length=%lu on router... from %s ",
                        length,
                        inet_ntop(AF_INET6, remote.sin6_addr.s6_addr, buff,
                                  sizeof(buff)));
                    print_buffer(buffer, length);
                    inet_ntop(AF_INET6, &remote, buff, sizeof(remote));
                    fprintf(stderr, "src %s\n", buff);
                    memcpy(&all_apps->src, &remote, sizeof(remote.sin6_addr));
                    all_apps->src_bfr_id =
                        get_id_from_address(&remote.sin6_addr, mapping);
                    // fprintf(stderr, "THE ID is %d\n", to_app.src_bfr_id);
                    bier_processing(buffer, length, bier, all_apps);
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

                    bier_message_type type;
                    void *decoded_message =
                        decode_application_message(unix_buffer, nb_read, &type);
                    if (!decoded_message) {
                        fprintf(stderr, "Confirmed\n");
                        break;
                    }

                    switch (type) {
                        case PACKET: {
                            if (process_unix_message_is_payload(decoded_message, bier, all_apps) < 0) {
                                goto error;
                            }
                            break;
                        }
                        case BIND: {
                            if (process_unix_message_is_bind(decoded_message, all_apps) < 0) {
                                goto error;
                            }
                            break;
                        }
                        default: {
                            fprintf(stderr, "confirmed");
                            goto error;
                        }
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
    close(sending_socket);
    close(listening_socket);
}
