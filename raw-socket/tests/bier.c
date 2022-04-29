#include <stdio.h>
#include "CUnit/Basic.h"
#include "../include/bier.h"

void test_set_bier_bsl()
{
    uint8_t buffer[20] = {};
    set_bier_bsl(buffer, 0xb);
    for (int i = 0; i < 20; ++i)
    {
        if (i == 5)
        {
            CU_ASSERT_EQUAL(buffer[i], 0xb0);
        }
        else
        {
            CU_ASSERT_EQUAL(buffer[i], 0);
        }
    }
}

void test_set_bier_proto()
{
    uint8_t buffer[20] = {};
    set_bier_proto(buffer, 5);
    for (int i = 0; i < 20; ++i)
    {
        if (i == 9)
        {
            CU_ASSERT_EQUAL(buffer[i] & 0x3f, 5);
        }
        else
        {
            CU_ASSERT_EQUAL(buffer[i], 0);
        }
    }
}

void test_set_bitstring_ptr()
{
    uint8_t buffer[20] = {};
    uint8_t bitstring[8] = {};
    for (int i = 0; i < 8; ++i)
    {
        bitstring[i] = i * 2;
    }

    set_bitstring_ptr(buffer, bitstring, 1);

    for (int i = 0; i < 12; ++i)
    {
        CU_ASSERT_EQUAL(buffer[i], 0);
    }
    for (int i = 12; i < 20; ++i)
    {
        CU_ASSERT_EQUAL(buffer[i], bitstring[i - 12]);
    }
}

void test_set_bitstring()
{
    uint8_t buffer[20] = {};
    uint64_t bitstring0 = 0xFB36D;
    uint64_t bitstring1 = 0xFF48;

    set_bitstring(buffer, 0, bitstring0);

    uint64_t bitstring_test0 = (uint64_t)buffer[12];

    CU_ASSERT_EQUAL(bitstring0, bitstring0);
}

void test_get_bift_id()
{
    uint32_t buffer[5] = {};
    uint32_t value = 0x45FD2;
    buffer[0] = htobe32((value << 12) & 0xfffff000);

    uint32_t bift_id = get_bift_id(buffer);
    CU_ASSERT_EQUAL(bift_id, value);
}

void test_get_bitstring_ptr()
{
    uint8_t buffer[20] = {};
    uint64_t bitstring = htobe64(0xCCCD315);
    uint64_t* bitstring_ptr = (uint64_t *)&buffer[12];
    bitstring_ptr[0] = bitstring;

    uint64_t *bitstring_ptr_test = get_bitstring_ptr(buffer);
    CU_ASSERT_EQUAL(bitstring_ptr_test[0], bitstring_ptr[0]);
}

void test_get_bitstring()
{
    uint8_t buffer[20] = {};
    uint64_t bitstring = 0xCCCD315;
    uint64_t* bitstring_ptr = (uint64_t *)&buffer[12];
    bitstring_ptr[0] = htobe64(bitstring);

    uint64_t bitstring_test = get_bitstring(buffer, 0);
    printf("Test %lx et nous %lx\n", bitstring_test, bitstring);
    CU_ASSERT_EQUAL(bitstring_test, bitstring);
}

int main()
{
    CU_initialize_registry();
    CU_pSuite bier_header_manip = CU_add_suite("BIER header manipulation", 0, 0);
    
    CU_add_test(bier_header_manip, "Set BIER BSL", test_set_bier_bsl);
    CU_add_test(bier_header_manip, "Set BIER proto", test_set_bier_proto);
    CU_add_test(bier_header_manip, "Set BIER Bitstring ptr", test_set_bitstring_ptr);
    CU_add_test(bier_header_manip, "Set bitstring", test_set_bitstring);
    CU_add_test(bier_header_manip, "Get bift", test_get_bift_id);
    CU_add_test(bier_header_manip, "Get bitstring ptr", test_get_bitstring_ptr);
    CU_add_test(bier_header_manip, "Get bitstring", test_get_bitstring);

    CU_basic_run_tests();
    CU_basic_show_failures(CU_get_failure_list());

    return 0;
}