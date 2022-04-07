#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void print_payload(uint8_t *payload, ssize_t payload_length)
{
    printf("Packet is %lu bytes long\n", payload_length);
    for (ssize_t i = 0; i < payload_length; ++i)
    {
        printf("%u", payload[i]);
        if (i < payload_length - 1)
        {
            printf(" ");
        }
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [udp port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int udp_port = strtoul(argv[1], NULL, 10);
    if (udp_port <= 0)
    {
        fprintf(stderr, "UDP port must be a positive integer\n");
        exit(EXIT_FAILURE);
    }

    int socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (socket_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // We expect packets to be sent from the loopback locally
    struct sockaddr_in6 loopback_addr = {};
    socklen_t socket_length = sizeof(loopback_addr);
    loopback_addr.sin6_family = AF_INET6;
    loopback_addr.sin6_port = htons(udp_port);
    if (inet_pton(AF_INET6, "::1", loopback_addr.sin6_addr.s6_addr) == 0)
    {
        perror("inet pton");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    if (bind(socket_fd, (struct sockaddr *)&loopback_addr, sizeof(loopback_addr)) == -1)
    {
        perror("bind");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "Start listening to port %u\n", udp_port);

    char packet[4096];
    int i = 0;
    while (i++ < 10)
    {
        memset(packet, 0, sizeof(packet));
        memset(packet, 1, sizeof(char) * 10);
        ssize_t received = recv(socket_fd, packet, sizeof(packet), 0);
        // ssize_t received = sendto(socket_fd, packet, 10, 0, (struct sockaddr *)&loopback_addr, socket_length);
        if (received == -1)
        {
            perror("recvfrom");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
        print_payload(packet, received);
    }

    close(socket_fd);
    exit(EXIT_SUCCESS);
}