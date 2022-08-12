#include "../include/qcbor-encoding.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

UsefulBufC encode_bier_payload(UsefulBuf Buffer,
                               const bier_payload_t *bier_payload) {
    // https://github.com/laurencelundblade/QCBOR/blob/master/example.c
    QCBOREncodeContext EncodeCtx;
    QCBOREncode_Init(&EncodeCtx, Buffer);
    QCBOREncode_OpenMap(&EncodeCtx);
    QCBOREncode_AddInt64ToMap(&EncodeCtx, "bift_id", bier_payload->use_bier_te);
    UsefulBufC bitstring_buf = {bier_payload->bitstring,
                                bier_payload->bitstring_length};
    QCBOREncode_AddBytesToMap(&EncodeCtx, "bitstring", bitstring_buf);
    UsefulBufC payload_buf = {bier_payload->payload,
                              bier_payload->payload_length};
    QCBOREncode_AddBytesToMap(&EncodeCtx, "payload", payload_buf);
    QCBOREncode_CloseMap(&EncodeCtx);

    UsefulBufC EncodedCBOR;
    QCBORError uErr;
    uErr = QCBOREncode_Finish(&EncodeCtx, &EncodedCBOR);
    if (uErr != QCBOR_SUCCESS) {
        return NULLUsefulBufC;
    } else {
        return EncodedCBOR;
    }
}

bier_payload_t *decode_bier_payload(QCBORDecodeContext ctx) {
    QCBORError uErr;
    QCBORItem item;

    bier_payload_t *bier_payload =
        (bier_payload_t *)malloc(sizeof(bier_payload_t));
    if (!bier_payload) {
        perror("malloc decode payload");
        return NULL;
    }

    QCBORDecode_GetInt64InMapSZ(&ctx, "bift_id", &(bier_payload->use_bier_te));
    QCBORDecode_GetInt64InMapSZ(&ctx, "proto", (uint64_t *)&bier_payload->proto);

    QCBORDecode_GetItemInMapSZ(&ctx, "bitstring", QCBOR_TYPE_BYTE_STRING,
                               &item);
    if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
        UsefulBufC bitstring_buf = item.val.string;
        bier_payload->bitstring_length = bitstring_buf.len;
        bier_payload->bitstring =
            (uint8_t *)malloc(sizeof(uint8_t) * bier_payload->bitstring_length);
        if (!bier_payload->bitstring) {
            perror("malloc");
        } else {
            memcpy(bier_payload->bitstring, bitstring_buf.ptr,
                   bitstring_buf.len);
        }
    }

    QCBORDecode_GetItemInMapSZ(&ctx, "payload", QCBOR_TYPE_BYTE_STRING, &item);
    if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
        UsefulBufC payload_buf = item.val.string;
        bier_payload->payload_length = payload_buf.len;
        bier_payload->payload =
            (uint8_t *)malloc(sizeof(uint8_t) * bier_payload->payload_length);
        if (0 && !bier_payload->payload) {
            perror("malloc");
        } else {
            memcpy(bier_payload->payload, payload_buf.ptr, payload_buf.len);
        }
    }

    uErr = QCBORDecode_GetError(&ctx);
    if (uErr != QCBOR_SUCCESS) {
        free(bier_payload);
        perror("decode message qcbor geterror");
        return NULL;
    }

    return bier_payload;
}

int encode_local_bier_payload(
    int socket, const bier_received_packet_t *bier_received_packet,
    const struct sockaddr_un *dest_addr, socklen_t addrlen) {
    // Make room for other information
    size_t qcbor_length = bier_received_packet->payload_length +
                          sizeof(bier_received_packet->ip6_encap_src) + 200;
    UsefulBuf_MAKE_STACK_UB(Buffer, qcbor_length);

    QCBOREncodeContext ctx;
    QCBOREncode_Init(&ctx, Buffer);
    QCBOREncode_OpenMap(&ctx);

    UsefulBufC payload_buf = {bier_received_packet->payload,
                              bier_received_packet->payload_length};
    QCBOREncode_AddBytesToMap(&ctx, "payload", payload_buf);

    UsefulBufC src_addr_buf = {&bier_received_packet->ip6_encap_src,
                               sizeof(bier_received_packet->ip6_encap_src)};
    QCBOREncode_AddBytesToMap(&ctx, "source_addr", src_addr_buf);

    QCBOREncode_AddInt64ToMap(&ctx, "upstream_bifr",
                              bier_received_packet->upstream_router_bfr_id);

    QCBOREncode_CloseMap(&ctx);

    UsefulBufC EncodedCBOR;
    QCBORError uErr;
    uErr = QCBOREncode_Finish(&ctx, &EncodedCBOR);
    if (uErr != QCBOR_SUCCESS) {
        // TODO: update errno
        fprintf(stderr, "L'erreur vient d'ici........\n");
        return -1;
    }

    size_t nb_sent = sendto(socket, EncodedCBOR.ptr, EncodedCBOR.len, 0,
                            (struct sockaddr *)dest_addr, addrlen);
    if (nb_sent < 0) {
        perror("encode_local_bier_payload sendto");
    }
    return nb_sent;
}

bier_bind_t *decode_bier_bind(QCBORDecodeContext *ctx) {
    QCBORError uErr;
    QCBORItem item;

    bier_bind_t *bind = (bier_bind_t *)malloc(sizeof(bier_bind_t));
    if (!bind) {
        perror("malloc decode bind");
        return NULL;
    }
    memset(bind, 0, sizeof(bier_bind_t));

    QCBORDecode_GetInt64InMapSZ(ctx, "proto", (uint64_t *)&bind->proto);
    uint64_t is_listener;
    QCBORDecode_GetInt64InMapSZ(ctx, "is_listener", &is_listener);
    bind->is_listener = is_listener == 1;

    QCBORDecode_GetItemInMapSZ(ctx, "unix_path", QCBOR_TYPE_BYTE_STRING, &item);
    if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
        UsefulBufC unix_path_buf = item.val.string;
        memcpy(bind->unix_path, unix_path_buf.ptr, unix_path_buf.len);
    }

    QCBORDecode_GetItemInMapSZ(ctx, "mc_sockaddr", QCBOR_TYPE_BYTE_STRING, &item);
    if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
        UsefulBufC mc_sockaddr_buf = item.val.string;
        memcpy(&bind->mc_sockaddr, mc_sockaddr_buf.ptr, mc_sockaddr_buf.len);
    }
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&bind->mc_sockaddr;
    fprintf(stderr, "Bound to2: ");
    for (int j = 0; j < 16; ++j) {
        fprintf(stderr, "%x ", addr->sin6_addr.s6_addr[j]);
    }
    fprintf(stderr, "\n");

    uErr = QCBORDecode_GetError(ctx);
    if (uErr != QCBOR_SUCCESS) {
        free(bind);
        perror("decode message qcbor geterror");
        return NULL;
    }

    return bind;
}

void *decode_application_message(void *app_buf, ssize_t len,
                                 bier_message_type *msg) {
    UsefulBufC buffer = {app_buf, len};
    QCBORDecodeContext ctx;
    QCBORError uErr;

    QCBORDecode_Init(&ctx, buffer, QCBOR_DECODE_MODE_NORMAL);
    QCBORDecode_EnterMap(&ctx, NULL);

    // Type indicator
    int64_t type;
    QCBORDecode_GetInt64InMapSZ(&ctx, "type", &type);
    *msg = type;

    switch (type) {
        case PACKET: {
            fprintf(stderr, "Will call PACKET decode\n");
            bier_payload_t *payload = decode_bier_payload(ctx);
            if (!payload) {
                return NULL;
            }
            fprintf(stderr, "Payload BIER information: %lu %lu\n",
                    payload->bitstring_length, payload->payload_length);

            QCBORDecode_ExitMap(&ctx);
            if (QCBORDecode_Finish(&ctx) != QCBOR_SUCCESS) {
                fprintf(stderr, "ERROR HERE\n");
                free(payload->bitstring);
                free(payload->payload);
                free(payload);
                return NULL;
            }

            return (void *)payload;
        }
        case BIND: {
            bier_bind_t *bind = decode_bier_bind(&ctx);
            if (!bind) {
                return NULL;
            }
            QCBORDecode_ExitMap(&ctx);
            if (QCBORDecode_Finish(&ctx) != QCBOR_SUCCESS) {
                free(bind);
                return NULL;
            }
            return (void *)bind;
        }
        default:
            fprintf(stderr, "Unsupported UNIX message type: %ld\n", type);
            QCBORDecode_ExitMap(&ctx);
            QCBORDecode_Finish(&ctx);
            return NULL;
    }
}