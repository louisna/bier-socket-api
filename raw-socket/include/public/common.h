#ifndef __BIER_COMMON_H_
#define __BIER_COMMON_H_

#include <netinet/ip6.h>

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

typedef enum {
    PACKET,
    BIND,
} bier_message_type;

typedef struct {
    char unix_path[NAME_MAX];     // Path to the UNIX socket of app using BIER
    uint16_t proto;               // Protocol following the BIER header
    struct sockaddr_in mc_sockaddr;  // Multicast source of interest socket address
} bier_bind_t;

#endif