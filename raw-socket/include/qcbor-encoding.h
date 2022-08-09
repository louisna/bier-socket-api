#include <netinet/ip6.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "public/common.h"
#include "qcbor/qcbor.h"
#include "qcbor/qcbor_decode.h"
#include "qcbor/qcbor_encode.h"
#include "qcbor/qcbor_spiffy_decode.h"

typedef struct {
    int64_t use_bier_te;
    int64_t bitstring_length;  // In bytes
    int64_t payload_length;
    uint8_t *payload;
    uint8_t *bitstring;
} bier_payload_t;

typedef struct {
    uint8_t *payload;
    uint64_t payload_length;
    struct in6_addr ip6_encap_src;
    struct in6_addr ip6_encap_dst;
    int64_t upstream_router_bfr_id;
} bier_received_packet_t;

/**
 * @brief Encodes a packet in QCBOR to send to the BIER daemon for Multicast
 * forwarding
 *
 * @param Buffer
 * @param bier_payload
 * @return UsefulBufC
 */
UsefulBufC encode_bier_payload(UsefulBuf Buffer,
                               const bier_payload_t *bier_payload);

// bier_payload_t *decode_bier_payload(QCBORDecodeContext *ctx);

/**
 * @brief Sends a QCBOR encoding of the received BIER packet that must be
 * processed by the local router Only used by the BIER daemon
 *
 * @param socket
 * @param bier_received_packet
 * @param dest_addr
 * @param addrlen
 * @return QCBORError
 */
int encode_local_bier_payload(
    int socket, const bier_received_packet_t *bier_received_packet,
    const struct sockaddr_un *dest_addr, socklen_t addrlen);

/**
 * @brief
 *
 * @param app_buf
 * @param len
 * @param msg
 * @return void* Pointer to (allocated) buffer with the decoded data. The caller
 * is responsible to cast in the correct type using the `msg` variable. Returns
 * NULL in case of error during the decoding
 */
void *decode_application_message(void *app_buf, ssize_t len,
                                 bier_message_type *msg);