#include <stdint.h>
#include "qcbor/qcbor.h"
#include "qcbor/qcbor_encode.h"
#include "qcbor/qcbor_decode.h"
#include "qcbor/qcbor_spiffy_decode.h"

typedef struct {
    int64_t use_bier_te;
    int64_t bitstring_length; // In bytes
    uint8_t *bitstring;
    int64_t payload_length;
    uint8_t *payload;
} bier_payload_t;


/**
 * @brief Encode a BIER payload in CBOR using the QCBOR library
 * 
 * @param Buffer 
 * @param bier_payload 
 * @return UsefulBufC 
 */
UsefulBufC encode_bier_payload(UsefulBuf Buffer, const bier_payload_t *bier_payload);

QCBORError decode_bier_payload(UsefulBufC buffer, bier_payload_t *bier_payload);