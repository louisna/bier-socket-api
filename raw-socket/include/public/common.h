#ifndef __BIER_COMMON_H_
#define __BIER_COMMON_H_

#include <netinet/ip6.h>
#include <stdbool.h>

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

typedef enum {
    PACKET,
    BIND,
} bier_message_type;

typedef struct {
    char unix_path[NAME_MAX];  // Path to the UNIX socket of app using BIER
    uint16_t proto;            // Protocol following the BIER header
    struct sockaddr_in6
        mc_sockaddr;   // Multicast source of interest socket address
    bool is_listener;  // True is the bind message concerns a multicast receiver
                       // False if it is a multicast sender (do not warn the sender)
} bier_bind_t;

/* BIER Next Protocol Identifiers */
#define BIERPROTO_RESERVED 0
#define BIERPROTO_MPLS_DOWN 1
#define BIERPROTO_MPLS_UP 2
#define BIERPROTO_ETH 3
#define BIERPROTO_IPV4 4
#define BIERPROTO_OAM 5
#define BIERPROTO_IPV6 6
/* 7-62 unassigned */
#define BIERPROTO_RESERVED_RAW 63

#endif