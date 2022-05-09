#ifndef BIER_H
#define BIER_H

#include <stdint.h>
#ifndef __APPLE__
#include <endian.h>
#endif

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
#include <netinet/udp.h>

#define get_bift_id(data) ( be32toh( (( ((uint32_t *)data)[0]))) >> 12 )
#define set_bier_bift_id(____data, ____bift_id)                                \
    {                                                                     \
        uint32_t *____d32 = (uint32_t *)____data;                         \
        ____d32[0] = (htobe32(____bift_id << 12)) + (____d32[0] & 0xfff); \
    }
#define get_bitstring(data, bitstring_idx) (htobe64(*((uint64_t *)&((uint32_t *)data)[3 + bitstring_idx])))
#define get_bitstring_ptr(data) ((uint64_t *)(&(data)[12]))
#define set_bitstring(data, bitstring_idx, bitstring) \
    {                                                 \
        uint32_t *d32 = (uint32_t *)data;             \
        uint64_t *d64 = (uint64_t *)&d32[3];          \
        d64[bitstring_idx] = htobe64(bitstring);      \
    }
#define set_bitstring_ptr(data, bitstring_ptr, bitstring_max_idx)         \
    {                                                                     \
        uint32_t *d32 = (uint32_t *)data;                                 \
        uint64_t *d64 = (uint64_t *)&d32[3];                              \
        memcpy(d64, bitstring_ptr, sizeof(uint64_t) * bitstring_max_idx); \
    }
#define set_bier_proto(d, proto)    \
    {                               \
        uint8_t *d8 = (uint8_t *)d; \
        d8[9] &= 0xc0;              \
        d8[9] |= (proto & 0x3f);    \
    }
#define set_bier_bsl(d, bsl)        \
    {                               \
        uint8_t *d8 = (uint8_t *)d; \
        d8[5] &= 0x0f;              \
        d8[5] |= (bsl << 4);        \
    }
#define get_entropy(d) (((uint16_t *)d)[5])
#define set_entropy(d, v)                  \
    {                                      \
        uint16_t *____d16 = (uint16_t *)d; \
        ____d16[5] = v;                    \
    }

/**
 * @brief Bits operations on the bitstring
 */
typedef enum
{
    bitwise_u64_and_not,
    bitwise_u64_and
} bitstring_operation;

typedef enum
{
    BIER = 1,
    BIER_TE = 2,
} bier_type;

/**
 * @brief Representation of an entry of the BIER Forwarding Table
 */
typedef struct
{
    uint64_t *forwarding_bitmask;     // The bitmask of this BFR entry
    uint32_t bitstring_length;        // Length of `forwarding_bitmask` in bits
    int32_t bfr_nei;                  // BIER Forwarding Router Neighbour - Decrepated
    struct sockaddr_in6 bfr_nei_addr; // Socket address to reach the BFR neighbors of this entry
} bier_bft_entry_ecmp_t;

typedef struct
{
    uint32_t bfr_id; // BFR-ID of the neighbour router in the network
    int nb_ecmp_entries;
    bier_bft_entry_ecmp_t **ecmp_entry;
} bier_bft_entry_t;

/**
 * @brief Representation of the state of a BIER Forwarding Router
 */
typedef struct
{
    uint32_t bift_id;
    int local_bfr_id;          // BFR-ID of the router in the network
    int nb_bft_entry;          // Number of entries in the BIER Forwarding Table
    uint32_t bitstring_length; // Represents the "BSL" in bits
    bier_bft_entry_t **bft;    // Table of length `nb_bft_entry` containing all entries of the BIER Forwarding Table
} bier_internal_t;

typedef struct
{
    uint32_t bift_id;
    int local_bfr_id;
    uint32_t bitstring_length;
    uint64_t *global_bitstring;
    int nb_adjacencies;
    struct sockaddr_in6 *bfr_nei_addr;
    int *adj_to_bp;
} bier_te_internal_t;

typedef struct
{
    bier_type t;
    union
    {
        bier_internal_t *bier;
        bier_te_internal_t *bier_te;
    };
} bier_bift_type_t;

typedef struct
{
    struct sockaddr_in6 local; // Socket address with the loopback address of the router
    int socket;                // Socket to send and receive packets
    int nb_bift;               // Number of different BIFT in the configuration
    bier_bift_type_t *b;
} bier_bift_t;

/**
 * @brief Local processing function when the router receives a packet belonging to itself.
 */
typedef struct
{
    void *args; // Additional arguments to provide to the `local_processing_function` function
    // Function that will be executed when the router receives a packet belonging to it.
    // `bier_packet` is the buffer containing the packet starting at the BIER header. The packet is of length `packet_length`
    // and the BIER header is of length `bier_header_length`. As a result, the content of the BIER packet is of length `packet_length` - `bier_header_length`.
    // The function takes an additional pointer with arguments that the application can provide to the function.
    void (*local_processing_function)(const uint8_t *bier_packet, const uint32_t packet_length, const uint32_t bier_header_length, void *args);
} bier_local_processing_t;

/**
 * @brief Read a BIER static configuration file to construct the local BIER Forwarding Table
 *
 * @param config_filepath path to the configuration file
 * @return bier_internal_t* structure containing the BFT information
 */
bier_bift_t *read_config_file(char *config_filepath);

/**
 * @brief Release the memory associated with the BIER Forwarding Table structure
 *
 * @param bft pointer to the BIER Forwarding Table structure
 */
void free_bier_bft(bier_bift_t *bft);

/**
 * @brief Process the packet given by *buffer* of length *buffer_length* using the BIER Forwarding Table *bft*.
 * For each packet whose destination is the local router processing the packet, the *bier_local_processing* structure
 * launches the local function of the structure. Each forwarding is done using the *socket_fd* raw socket
 *
 * @param buffer pointer to the buffer - should start with the BIER header
 * @param buffer_length length of the *buffer*
 * @param bft the BIER Forwarding Table
 * @param bier_local_processing structure containing the function and additional arguments to handle a local packet
 * @return int error indication state
 */
int bier_non_te_processing(uint8_t *buffer, size_t buffer_length, bier_internal_t *bft, int socket, bier_local_processing_t *bier_local_processing);

/**
 * @brief Same as bier_processing but using the BIER-TE processing
 *
 * @param buffer see bier_processing
 * @param buffer_length see bier_processing
 * @param bft see bier_processing
 * @param bier_local_processing see bier_processing
 * @return int error indication state
 */
int bier_te_processing(uint8_t *buffer, size_t buffer_length, bier_te_internal_t *bft, int socket, bier_local_processing_t *bier_local_processing);

int bier_processing(uint8_t *buffer, size_t buffer_length, bier_bift_t *bier, bier_local_processing_t *bier_local_processing);

/**
 * @brief Prints to the standard output the content of the BIER Forwarding table `bft`.
 *
 * @param bft the BIER Forwarding Table to display.
 */
void print_bft(bier_internal_t *bft);

#endif // BIER_H
