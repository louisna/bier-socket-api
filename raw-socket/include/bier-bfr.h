#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip6.h>
#include <unistd.h>
#include <signal.h>

#include "bier.h"

typedef struct
{
    uint32_t bfr_id;
    uint32_t forwarding_bitmask;
    int32_t bfr_nei; // BIER Forwarding Router Neighbour
    struct sockaddr_in6 bfr_nei_addr;
} bier_bft_entry_t;

typedef struct
{
    int local_bfr_id;
    struct in6_addr local;
    int nb_bft_entry;
    int socket;
    bier_bft_entry_t **bft;
} bier_internal_t;