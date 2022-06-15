#include <stdio.h>
#include <stdlib.h>
#include "CUnit/Basic.h"
#include "../include/qcbor-encoding.h"

void test_encoding_decoding() {
    UsefulBuf_MAKE_STACK_UB(Buffer, 300);

    UsefulBufC EncodedEngine;
    bier_payload_t *bier = (bier_payload_t *)malloc(sizeof(bier_payload_t));
    uint8_t *bitstring = (uint8_t *)malloc(sizeof(uint8_t) * 20);
    uint8_t *payload = (uint8_t *)malloc(sizeof(uint8_t) * 100);
    memset(bitstring, 5, sizeof(uint8_t) * 20);
    memset(payload, 2, sizeof(uint8_t) * 100);

    bier->use_bier_te = 0;
    bier->payload_length = 100;
    bier->bitstring_length = 20;
    bier->payload = payload;
    bier->bitstring = bitstring;

    EncodedEngine = encode_bier_payload(Buffer, bier);
    CU_ASSERT_FALSE(UsefulBuf_IsNULLC(EncodedEngine));

    bier_payload_t bier_output;
    memset(&bier_output, 0, sizeof(bier_payload_t));
    QCBORError uErr = decode_bier_payload(EncodedEngine, &bier_output);

    CU_ASSERT_EQUAL(bier->bitstring_length, bier_output.bitstring_length);
    CU_ASSERT_EQUAL(bier->payload_length, bier_output.payload_length);

    for (int i = 0; i < bier->bitstring_length; ++i) {
        CU_ASSERT_EQUAL(bier->bitstring[i], bier_output.bitstring[i]);
    }

    for (int i = 0; i < bier->payload_length; ++i) {
        CU_ASSERT_EQUAL(bier->payload[i], bier_output.payload[i]);
    }
}

int main() {
    CU_initialize_registry();
    CU_pSuite bier_header_manip = CU_add_suite("QCBOR encoding and decoding", 0, 0);
    
    CU_add_test(bier_header_manip, "Encode/Decode with QCBOR", test_encoding_decoding);

    CU_basic_run_tests();
    CU_basic_show_failures(CU_get_failure_list());

    return 0;
}