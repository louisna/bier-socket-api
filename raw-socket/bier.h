#include <stdint.h>
#ifndef __APPLE__
#include <endian.h>
#endif

typedef struct
{
    uint32_t bift_id: 20;
    uint8_t tc: 3;
    uint8_t s: 1;
    uint8_t ttl;
    uint8_t nibble: 4;
    uint8_t ver: 4;
    uint8_t bsl: 4;
    uint32_t entropy: 20;
    uint8_t oam: 2;
    uint8_t rsv: 2;
    uint8_t dscp: 6;
    uint8_t proto: 6;
    uint16_t bfir_id;
    uint32_t bitstring[0];
} __attribute__((packed)) bier_header_t;

#define get_bift_id(data) ( be32toh(data[0]) >> 12 )
#define get_bitstring(data, bitstring_idx) ( htobe32(((uint32_t *)data)[3 + bitstring_idx]) )
#define set_bitstring(data, bitstring_idx, bitstring) { uint32_t *d32 = (uint32_t *)data; d32[3 + bitstring_idx] = htobe32(bitstring); }
#define set_bier_proto(d, proto) { uint8_t *d8 = (uint8_t *)&d[2]; d8[1] &= 0xc0; d8[1] |= (proto & 0x3f); }
#define set_bier_bsl(d, bsl) { uint8_t *d8 = (uint8_t *)&d[1]; d8[1] &= 0x0f; d8[1] |= (bsl << 4); }