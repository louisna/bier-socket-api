#include <stdint.h>
#include <netinet/ip6.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "qcbor/qcbor.h"
#include "qcbor/qcbor_encode.h"
#include "qcbor/qcbor_decode.h"
#include "qcbor/qcbor_spiffy_decode.h"

typedef struct
{
    int64_t use_bier_te;
    int64_t bitstring_length; // In bytes
    uint8_t *bitstring;
    int64_t payload_length;
    uint8_t *payload;
} bier_payload_t;

typedef struct
{
    uint8_t *payload;
    uint64_t payload_length;
    struct in6_addr ip6_encap_src;
    struct in6_addr ip6_encap_dst;
} bier_received_packet_t;

/**
 * @brief Encodes a packet in QCBOR to send to the BIER daemon for Multicast forwarding
 *
 * @param Buffer
 * @param bier_payload
 * @return UsefulBufC
 */
UsefulBufC encode_bier_payload(UsefulBuf Buffer, const bier_payload_t *bier_payload);

/**
 * @brief Decodes a packet with BIER information for Multicast forwarding. Only used by the BIER daemon
 *
 * @param buffer
 * @param bier_payload
 * @return QCBORError
 */
QCBORError decode_bier_payload(UsefulBufC buffer, bier_payload_t *bier_payload);

/**
 * @brief Sends a QCBOR encoding of the received BIER packet that must be processed by the local router
 * Only used by the BIER daemon
 * 
 * @param socket 
 * @param bier_received_packet 
 * @param dest_addr 
 * @param addrlen 
 * @return QCBORError 
 */
int encode_local_bier_payload(int socket, const bier_received_packet_t *bier_received_packet, const struct sockaddr_un *dest_addr, socklen_t addrlen);
