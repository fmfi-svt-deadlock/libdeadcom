#include <stdlib.h>
#include <string.h>
#include "unity.h"

#include "leaky-pipe.h"


void test_Transfers() {
    lp_args_t args;
    leaky_pipe_t lp;
    lp_init_args(&args);
    lp_init(&lp, &args);

    uint8_t orig[20] = { (uint8_t)(rand() % 256) };
    for (unsigned int i = 0; i < sizeof(orig); i++) {
        lp_transmit(&lp, orig[i]);
    }
    pipe_producer_free(lp.pipe_producer);

    uint8_t rcvd[30];
    TEST_ASSERT_EQUAL(20, lp_receive(&lp, rcvd, sizeof(rcvd)));
    TEST_ASSERT_EQUAL_MEMORY(orig, rcvd, 20);
}


void test_Drops() {
    unsigned int dl[] = {1, 3};

    lp_args_t args;
    leaky_pipe_t lp;
    lp_init_args(&args);
    args.drop_list = dl;
    args.drop_list_len = 2;
    lp_init(&lp, &args);

    uint8_t orig[] = {0, 1, 2, 3, 4, 5};
    for (unsigned int i = 0; i < sizeof(orig); i++) {
        lp_transmit(&lp, orig[i]);
    }
    pipe_producer_free(lp.pipe_producer);

    uint8_t rcvd[10];
    uint8_t expected[] = {0, 2, 4, 5};
    TEST_ASSERT_EQUAL(sizeof(expected), lp_receive(&lp, rcvd, sizeof(rcvd)));
    TEST_ASSERT_EQUAL_MEMORY(expected, rcvd, sizeof(expected));
}


void test_Corrupts() {
    lp_corrupt_def_t cl[] = {
        {.position = 1, .value = 0x20},
        {.position = 3, .value = 0x02},
        {.position = 3, .value = 0x20}
    };

    lp_args_t args;
    leaky_pipe_t lp;
    lp_init_args(&args);
    args.corrupt_list = cl;
    args.corrupt_list_len = 3;
    lp_init(&lp, &args);

    uint8_t orig[] = {0, 1, 2, 3, 4, 5};
    for (unsigned int i = 0; i < sizeof(orig); i++) {
        lp_transmit(&lp, orig[i]);
    }
    pipe_producer_free(lp.pipe_producer);

    uint8_t rcvd[10];
    uint8_t expected[] = {0, 1^0x20, 2, 3^0x22, 4, 5};
    TEST_ASSERT_EQUAL(sizeof(expected), lp_receive(&lp, rcvd, sizeof(rcvd)));
    TEST_ASSERT_EQUAL_MEMORY(expected, rcvd, sizeof(expected));
}


void test_Adds() {
    lp_corrupt_def_t al[] = {
        {.position = 0, .value = 0x47},
        {.position = 0, .value = 0x42},
        {.position = 2, .value = 0x02},
        {.position = 3, .value = 0x20}
    };

    lp_args_t args;
    leaky_pipe_t lp;
    lp_init_args(&args);
    args.add_list = al;
    args.add_list_len = 4;
    lp_init(&lp, &args);

    uint8_t orig[] = {0, 1, 2, 3, 4, 5};
    for (unsigned int i = 0; i < sizeof(orig); i++) {
        lp_transmit(&lp, orig[i]);
    }
    pipe_producer_free(lp.pipe_producer);

    uint8_t rcvd[20];
    uint8_t expected[] = {0x47, 0x42, 0, 1, 0x02, 2, 0x20, 3, 4, 5};
    TEST_ASSERT_EQUAL(sizeof(expected), lp_receive(&lp, rcvd, sizeof(rcvd)));
    TEST_ASSERT_EQUAL_MEMORY(expected, rcvd, sizeof(expected));
}


void test_CombinedFaults() {
    unsigned int dl[] = {2, 3};
    lp_corrupt_def_t cl[] = {
        {.position = 0, .value = 20},
        {.position = 3, .value = 20}
    };
    lp_corrupt_def_t al[] = {
        {.position = 0, .value = 0xF0},
        {.position = 0, .value = 0x0D},
        {.position = 4, .value = 0xFF}
    };

    lp_args_t args;
    leaky_pipe_t lp;
    lp_init_args(&args);
    args.drop_list = dl;
    args.drop_list_len = 2;
    args.corrupt_list = cl;
    args.corrupt_list_len = 2;
    args.add_list = al;
    args.add_list_len = 3;
    lp_init(&lp, &args);

    uint8_t orig[] = {0x10, 0x15, 0x20, 0x25, 0x30};
    for (unsigned int i = 0; i < sizeof(orig); i++) {
        lp_transmit(&lp, orig[i]);
    }
    pipe_producer_free(lp.pipe_producer);

    uint8_t rcvd[10];
    uint8_t expected[] = {0xF0, 0x0D, 0x04, 0x15, 0xFF, 0x30};
    TEST_ASSERT_EQUAL(sizeof(expected), lp_receive(&lp, rcvd, sizeof(rcvd)));
    TEST_ASSERT_EQUAL_MEMORY(expected, rcvd, sizeof(expected));
}


void test_RandomDrops() {
    lp_args_t args;
    leaky_pipe_t lp;
    lp_init_args(&args);
    args.drop_prob = 0.5;
    lp_init(&lp, &args);

    uint8_t orig[20] = { (uint8_t)(rand() % 256) };
    for (unsigned int i = 0; i < sizeof(orig); i++) {
        lp_transmit(&lp, orig[i]);
    }
    pipe_producer_free(lp.pipe_producer);

    uint8_t rcvd[30];
    TEST_ASSERT(20 > lp_receive(&lp, rcvd, sizeof(rcvd)));
}


void test_RandomCorrupts() {
    lp_args_t args;
    leaky_pipe_t lp;
    lp_init_args(&args);
    args.corrupt_prob = 0.5;
    lp_init(&lp, &args);

    uint8_t orig[20] = { (uint8_t)(rand() % 256) };
    for (unsigned int i = 0; i < sizeof(orig); i++) {
        lp_transmit(&lp, orig[i]);
    }
    pipe_producer_free(lp.pipe_producer);

    uint8_t rcvd[30];
    TEST_ASSERT_EQUAL(20, lp_receive(&lp, rcvd, sizeof(rcvd)));
    TEST_ASSERT(memcmp(orig, rcvd, 20) != 0);
}


void test_RandomAdds() {
    lp_args_t args;
    leaky_pipe_t lp;
    lp_init_args(&args);
    args.add_prob = 0.2;
    lp_init(&lp, &args);

    uint8_t orig[20] = { (uint8_t)(rand() % 256) };
    for (unsigned int i = 0; i < sizeof(orig); i++) {
        lp_transmit(&lp, orig[i]);
    }
    pipe_producer_free(lp.pipe_producer);

    uint8_t rcvd[50];
    TEST_ASSERT(20 < lp_receive(&lp, rcvd, sizeof(rcvd)));
}
