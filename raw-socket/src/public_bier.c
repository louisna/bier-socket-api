#include <errno.h>
#include <stdio.h>
#include "../include/public/bier.h"
#include "qcbor/qcbor.h"
#include "qcbor/qcbor_decode.h"
#include "qcbor/qcbor_encode.h"
#include "qcbor/qcbor_spiffy_decode.h"

ssize_t sendto_bier(int socket, const void *buf, size_t len,
                    const struct sockaddr *dest_addr, socklen_t addrlen,
                    uint16_t proto, bier_info_t *bier_info) {
    size_t qcbor_length = len + bier_info->send_info.bitstring_length +
                          200;  // Make room for other information encoding
    UsefulBuf_MAKE_STACK_UB(Buffer, qcbor_length);

    QCBOREncodeContext ctx;
    fprintf(stderr, "First few bytes of bitstring: %x %x %x\n", bier_info->send_info.bitstring[0], bier_info->send_info.bitstring[1], bier_info->send_info.bitstring[2]);
    QCBOREncode_Init(&ctx, Buffer);
    QCBOREncode_OpenMap(&ctx);
    QCBOREncode_AddInt64ToMap(&ctx, "type", PACKET);
    QCBOREncode_AddInt64ToMap(&ctx, "bift_id", bier_info->send_info.bift_id);
    UsefulBufC bitstring_buf = {bier_info->send_info.bitstring,
                                bier_info->send_info.bitstring_length};
    QCBOREncode_AddBytesToMap(&ctx, "bitstring", bitstring_buf);
    UsefulBufC payload_buf = {buf, len};
    QCBOREncode_AddBytesToMap(&ctx, "payload", payload_buf);
    QCBOREncode_AddInt64ToMap(&ctx, "proto", proto);
    QCBOREncode_CloseMap(&ctx);

    UsefulBufC EncodedCBOR;
    QCBORError uErr;
    uErr = QCBOREncode_Finish(&ctx, &EncodedCBOR);
    if (uErr != QCBOR_SUCCESS) {
        fprintf(stderr, "sendto_bier QCBOR error\n");
        return -1;
    }

    size_t nb_sent =
        sendto(socket, EncodedCBOR.ptr, EncodedCBOR.len, 0, dest_addr, addrlen);
    return nb_sent;
}

ssize_t recvfrom_bier(int socket, void *buf, size_t len,
                      struct sockaddr *src_addr, socklen_t *addrlen, bier_info_t *bier_info) {
    memset(bier_info, 0, sizeof(bier_info_t));
    ssize_t nb_read_return = 0;

    // Read the QCBOR content from the UNIX socket from the BIER daemon
    // TODO: quid if `buf` is longer than `tm_buf`: should not send "too small
    // buffer"
    uint8_t tmp_buf[4096];
    ssize_t nb_read = recv(socket, tmp_buf, sizeof(tmp_buf), 0);
    if (nb_read < 0) {
        perror("read unix socket bier");
        return nb_read;
    }
    fprintf(stderr, "local received: %ld\n", nb_read);

    // QCBOR decoding the data to make it "recvfrom" compatible
    UsefulBufC cbor = {tmp_buf, nb_read};
    QCBORDecodeContext ctx;
    QCBORError uErr;
    QCBORItem item;

    QCBORDecode_Init(&ctx, cbor, QCBOR_DECODE_MODE_NORMAL);

    // Payload and payload length
    QCBORDecode_EnterMap(&ctx, NULL);
    QCBORDecode_GetItemInMapSZ(&ctx, "payload", QCBOR_TYPE_BYTE_STRING, &item);
    if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
        UsefulBufC payload_buf = item.val.string;
        if (payload_buf.len > len) {
            // TODO: update errno
            return -1;
        }
        memcpy(buf, payload_buf.ptr, payload_buf.len);
        nb_read_return = payload_buf.len;
    }

    // Source address of the neighbor sending the packet
    QCBORDecode_GetItemInMapSZ(&ctx, "source_addr", QCBOR_TYPE_BYTE_STRING,
                               &item);
    if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
        UsefulBufC addr_buf = item.val.string;
        // TODO: check that it is not too long
        struct sockaddr_in6 *src_addr6 = (struct sockaddr_in6 *)src_addr;
        memcpy(&src_addr6->sin6_addr, addr_buf.ptr, addr_buf.len);
        *addrlen = addr_buf.len;
    }

    // BIER-ID of the upstream router of the packet
    QCBORDecode_GetInt64InMapSZ(&ctx, "upstream_bifr", &bier_info->recv_info.upstream_router_bfr_id);
    // fprintf(stderr, "------ AU DECODAGE on a %lu\n", bier_info->recv_info.upstream_router_bfr_id);
    
    QCBORDecode_ExitMap(&ctx);

    return nb_read_return;
}

int bind_bier(int socket, const struct sockaddr_un *bier_sock_path, bier_bind_t *bind_to) {
    size_t qcbor_length = sizeof(bier_bind_t) + 200; // Make room for other information encoding
    UsefulBuf_MAKE_STACK_UB(Buffer, qcbor_length);

    QCBOREncodeContext ctx;
    QCBOREncode_Init(&ctx, Buffer);
    QCBOREncode_OpenMap(&ctx);

    // Indicator
    QCBOREncode_AddInt64ToMap(&ctx, "type", BIND);

    QCBOREncode_AddInt64ToMap(&ctx, "proto", (uint64_t)bind_to->proto);
    UsefulBufC unix_path_buf = {bind_to->unix_path, NAME_MAX};
    QCBOREncode_AddBytesToMap(&ctx, "unix_path", unix_path_buf);
    UsefulBufC mc_sockaddr_buf = {&bind_to->mc_sockaddr, sizeof(struct sockaddr_in6)};
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&bind_to->mc_sockaddr;
    fprintf(stderr, "Bound to2: ");
    for (int j = 0; j < 16; ++j) {
        fprintf(stderr, "%x ", addr->sin6_addr.s6_addr[j]);
    }
    QCBOREncode_AddBytesToMap(&ctx, "mc_sockaddr", mc_sockaddr_buf);
    QCBOREncode_CloseMap(&ctx);

    UsefulBufC EncodedCBOR;
    QCBORError uErr;
    uErr = QCBOREncode_Finish(&ctx, &EncodedCBOR);
    if (uErr != QCBOR_SUCCESS) {
        return -1;
    }

    size_t nb_sent =
        sendto(socket, EncodedCBOR.ptr, EncodedCBOR.len, 0, (struct sockaddr *)bier_sock_path, sizeof(struct sockaddr_un));
    if (nb_sent < 0) {
        perror("Cannot send bind information to BIER:");
        return -1;
    }
    return 0;
}