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

typedef union {
    struct sockaddr_in6 v6;
    struct sockaddr_in v4;
    struct sockaddr_storage _storage;
} sockaddr_uniform_t;

typedef struct {
    char unix_path[NAME_MAX];  // Path to the UNIX socket of app using BIER
    uint16_t proto;            // Protocol following the BIER header
    sockaddr_uniform_t mc_sockaddr;   // Multicast source of interest socket address
    bool is_listener;  // True if the bind message concerns a multicast receiver
                       // False if it is a multicast sender (do not warn the sender)
    bool is_join; // True if the bind message concerns an MC join
                  // False if the bind message concerns an MC leave
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