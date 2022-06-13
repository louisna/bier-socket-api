#include <sys/un.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, )
    }
    int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (socket_fd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un dst = {};
    dst.sun_family = AF_UNIX;
    strcpy(dst.sun_path, "/tmp/socket-bfr");
    int data_len = strlen(dst.sun_path) + sizeof(dst.sun_family);

    char *data = "Hello, UNIX!";

    if (sendto(socket_fd, data, sizeof(char) * (strlen(data) + 1), 0, (struct sockaddr *)&dst, sizeof(struct sockaddr_un)) == -1)
    {
        perror("sendto");
        exit(EXIT_FAILURE);
    }

    close(socket_fd);
    exit(EXIT_SUCCESS);
}