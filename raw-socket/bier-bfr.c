#include "include/bier.h"
#include "include/local-processing.h"
#include <sys/un.h>
#include <errno.h>
#include <poll.h>
#include "include/qcbor-encoding.h"
#include "bier-sender.h"
#include <fcntl.h>

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

void free_bier_payload(bier_payload_t *bier_payload) {
    free(bier_payload->bitstring);
    free(bier_payload->payload);
    free(bier_payload);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <config_file> <unix_socket_path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *filename = argv[1];
    bier_bift_t *bier = read_config_file(filename);
    if (!bier)
    {
        exit(EXIT_FAILURE);
    }

    const char *unix_socket_path = argv[2];

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

    // print_bft(bier);

    // Open UNIX socket
    int unix_socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (unix_socket_fd == -1)
    {
        perror("Unix socket");
        exit(EXIT_FAILURE);
    }

    // TODO: path for UNIX socket dependent of the router name
    struct sockaddr_un unix_local = {};
    unix_local.sun_family = AF_UNIX;
    strcpy(unix_local.sun_path, unix_socket_path);
    int data_len = strlen(unix_local.sun_path) + sizeof(unix_local.sun_family);

    // https://medium.com/swlh/getting-started-with-unix-domain-sockets-4472c0db4eb1
    if (remove(unix_socket_path) == -1 && errno != ENOENT) {
        perror("Remove unix socket path");
        close(unix_socket_fd);
        exit(EXIT_FAILURE);
    }

    if (bind(unix_socket_fd, (struct sockaddr *)&unix_local, sizeof(struct sockaddr_un)) == -1)
    {
        perror("Bind unix socket");
        close(unix_socket_fd);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Bound to UNIX socket!\n");

    // Allocate poll fds
    int nfds = 2;
    struct pollfd *pfds = (struct pollfd *)calloc(nfds, sizeof(struct pollfd));
    if (!pfds)
    {
        perror("Calloc pfds");
        close(unix_socket_fd);
        exit(EXIT_FAILURE);
    }

    pfds[0].fd = bier->socket;
    pfds[1].fd = unix_socket_fd;

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
    if (!unix_buffer)
    {
        perror("malloc unix buffer");
    }
    memset(unix_buffer, 0, unix_buffer_size);

    while (1)
    {
        fprintf(stderr, "About to poll...\n");
        int ready = poll(pfds, nfds, -1);
        if (ready == -1)
        {
            perror("Poll");
            break;
        }

        fprintf(stderr, "Ready: %d\n", ready);
        for (int i = 0; i < nfds; ++i)
        {
            if (pfds[i].revents & POLLIN)
            {
                fprintf(stderr, "Got a message from %d!\n", i);
                if (i == 0)
                {
                    fprintf(stderr, "BIER socket\n");
                    memset(buffer, 0, sizeof(uint8_t) * buffer_size);
                    char buff[100];
                    size_t length = recvfrom(pfds[i].fd, buffer, sizeof(uint8_t) * buffer_size, 0, (struct sockaddr *)&remote, &remote_len);
                    fprintf(stderr, "Received packet of length=%lu on router... from %s ", length, inet_ntop(AF_INET6, remote.sin6_addr.s6_addr, buff, sizeof(buff)));
                    // fprintf(stderr, "The BITSTIRNG on router %u is %x\n", bier->local_bfr_id, buffer[19]);
                    print_buffer(buffer, length);
                    raw_args.src = remote.sin6_addr;
                    inet_ntop(AF_INET6, &remote, buff, sizeof(remote));
                    fprintf(stderr, "src %s\n", buff);
                    bier_processing(buffer, length, bier, &local_bier_processing);
                }
                else
                {
                    fprintf(stderr, "UNIX socket\n");
                    // TODO: 
                    ssize_t nb_read = recv(pfds[i].fd, unix_buffer, unix_buffer_size, 0);
                    if (nb_read < 0)
                    {
                        perror("read");
                        break;
                    }
                    fprintf(stderr, "Received a message of length: %lu\n", nb_read);

                    // Convert to recover the BIER packet
                    bier_payload_t *bier_payload = (bier_payload_t *)malloc(sizeof(bier_payload_t));
                    if (!bier_payload) {
                        perror("malloc bier payload");
                        break;
                    }
                    UsefulBufC cbor = {unix_buffer, nb_read};
                    QCBORError uErr = decode_bier_payload(cbor, bier_payload);
                    // TODO: check error

                    fprintf(stderr, "BIER payload of %lu bytes\n", bier_payload->payload_length);
                    fprintf(stderr, "BIER bitstring: ");
                    for (int i = 0; i < bier_payload->bitstring_length; ++i) {
                        fprintf(stderr, "%x ", bier_payload->bitstring[i]);
                    }
                    fprintf(stderr, "\n");
                    // TODO: proto must be sent also, and BIFT-ID based on TE?
                    bier_header_t *bh = init_bier_header((const uint64_t *)bier_payload->bitstring, bier_payload->bitstring_length * 8, 6, bier_payload->use_bier_te);
                    // TODO: check error
                    my_packet_t *packet = encap_bier_packet(bh, bier_payload->payload_length, bier_payload->payload);
                    int err = bier_processing(packet->packet, packet->packet_length, bier, &local_bier_processing);
                    if (err < 0) {
                        fprintf(stderr, "Error when processing the BIER packet at the router... exiting...\n");
                        my_packet_free(packet);
                        break;
                    }
                    free_bier_payload(bier_payload);
                    release_bier_header(bh);
                }
            }
            else if (pfds[i].revents != 0)
            {
                printf("  fd=%d; events: %s%s%s\n", pfds[i].fd,
                               (pfds[i].revents & POLLIN)  ? "POLLIN "  : "",
                               (pfds[i].revents & POLLHUP) ? "POLLHUP " : "",
                               (pfds[i].revents & POLLERR) ? "POLLERR " : "");
            }
        }
    }

    free(buffer);
    free(unix_buffer);
    fprintf(stderr, "Closing the program on router\n");
    free_bier_bft(bier);
    close(unix_socket_fd);
}
