#include "../include/public/bier.h"

ssize_t sendto_bier(int socket, const void *buf, size_t len,
                    const struct sockaddr *dest_addr, socklen_t addrlen,
                    bier_info_t *bier_info) {
    size_t qcbor_length = len + bier_info->send_info.bitstring_length +
                          200;  // Make room for other information encoding
    UsefulBuf_MAKE_STACK_UB(Buffer, qcbor_length);

    QCBOREncodeContext ctx;
    QCBOREncode_Init(&ctx, Buffer);
    QCBOREncode_OpenMap(&ctx);
    QCBOREncode_AddInt64ToMap(&ctx, "UseBierTE", bier_info->send_info.bift_id);
    UsefulBufC bitstring_buf = {bier_info->send_info.bitstring,
                                bier_info->send_info.bitstring_length};
    QCBOREncode_AddBytesToMap(&ctx, "BitString", bitstring_buf);
    UsefulBufC payload_buf = {buf, len};
    QCBOREncode_AddBytesToMap(&ctx, "Payload", payload_buf);
    QCBOREncode_CloseMap(&ctx);

    UsefulBufC EncodedCBOR;
    QCBORError uErr;
    uErr = QCBOREncode_Finish(&ctx, &EncodedCBOR);
    if (uErr != QCBOR_SUCCESS) {
        // TODO: update errno
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

    // QCBOR decoding the data to make it "recvfrom" compatible
    UsefulBufC cbor = {tmp_buf, nb_read};
    QCBORDecodeContext ctx;
    QCBORError uErr;
    QCBORItem item;

    QCBORDecode_Init(&ctx, cbor, QCBOR_DECODE_MODE_NORMAL);

    // Payload and payload length
    QCBORDecode_EnterMap(&ctx, NULL);
    QCBORDecode_GetItemInMapSZ(&ctx, "Payload", QCBOR_TYPE_BYTE_STRING, &item);
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
    QCBORDecode_GetItemInMapSZ(&ctx, "SourceAddr", QCBOR_TYPE_BYTE_STRING,
                               &item);
    if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
        UsefulBufC addr_buf = item.val.string;
        // TODO: check that it is not too long
        memcpy(src_addr, addr_buf.ptr, addr_buf.len);
        *addrlen = addr_buf.len;
    }

    // BIER-ID of the upstream router of the packet
    QCBORDecode_GetInt64InMapSZ(&ctx, "UpStreamBfrId", &bier_info->recv_info.upstream_router_bfr_id);
    // fprintf(stderr, "------ AU DECODAGE on a %lu\n", bier_info->recv_info.upstream_router_bfr_id);
    
    QCBORDecode_ExitMap(&ctx);

    return nb_read_return;
}