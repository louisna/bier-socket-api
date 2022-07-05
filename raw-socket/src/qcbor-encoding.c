#include "../include/qcbor-encoding.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

UsefulBufC encode_bier_payload(UsefulBuf Buffer, const bier_payload_t *bier_payload) {
    // https://github.com/laurencelundblade/QCBOR/blob/master/example.c
    QCBOREncodeContext EncodeCtx;
    QCBOREncode_Init(&EncodeCtx, Buffer);
    QCBOREncode_OpenMap(&EncodeCtx);
    QCBOREncode_AddInt64ToMap(&EncodeCtx, "UseBierTE", bier_payload->use_bier_te);
    UsefulBufC bitstring_buf = {bier_payload->bitstring, bier_payload->bitstring_length};
    QCBOREncode_AddBytesToMap(&EncodeCtx, "BitString", bitstring_buf);
    UsefulBufC payload_buf = {bier_payload->payload, bier_payload->payload_length};
    QCBOREncode_AddBytesToMap(&EncodeCtx, "Payload", payload_buf);
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

QCBORError decode_bier_payload(UsefulBufC buffer, bier_payload_t *bier_payload) {
    QCBORDecodeContext ctx;
    QCBORError uErr;
    QCBORItem item;

    QCBORDecode_Init(&ctx, buffer, QCBOR_DECODE_MODE_NORMAL);

    QCBORDecode_EnterMap(&ctx, NULL);
    QCBORDecode_GetInt64InMapSZ(&ctx, "UseBierTE", &(bier_payload->use_bier_te));

    QCBORDecode_GetItemInMapSZ(&ctx, "BitString", QCBOR_TYPE_BYTE_STRING, &item);
    if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
        UsefulBufC bitstring_buf = item.val.string;
        bier_payload->bitstring_length = bitstring_buf.len;
        bier_payload->bitstring = (uint8_t *)malloc(sizeof(uint8_t) * bier_payload->bitstring_length);
        if (!bier_payload->bitstring) {
            perror("malloc");
        } else {
            memcpy(bier_payload->bitstring, bitstring_buf.ptr, bitstring_buf.len);
        }
    }
    
    QCBORDecode_GetItemInMapSZ(&ctx, "Payload", QCBOR_TYPE_BYTE_STRING, &item);
    if (item.uDataType == QCBOR_TYPE_BYTE_STRING) {
        UsefulBufC payload_buf = item.val.string;
        bier_payload->payload_length = payload_buf.len;
        bier_payload->payload = (uint8_t *)malloc(sizeof(uint8_t) * bier_payload->payload_length);
        if (!bier_payload->payload) {
            perror("malloc");
        } else {
            memcpy(bier_payload->payload, payload_buf.ptr, payload_buf.len);
        }
    }
    
    uErr = QCBORDecode_GetError(&ctx);
    if(uErr != QCBOR_SUCCESS) {
        return uErr;
    }

    QCBORDecode_ExitMap(&ctx);
    
    uErr = QCBORDecode_Finish(&ctx);
    return uErr;
}