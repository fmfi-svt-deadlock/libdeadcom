#include <stdlib.h>
#include "unity.h"

#include "leaky-pipe.h"


void test_FlawlessPipe() {
    lp_args_t args;
    leaky_pipe_t lp;
    lp_init_args(&args);
    lp_init(&lp, &args);

    uint8_t orig[20] = { (uint8_t)(rand() % 256) };
    for (unsigned int i = 0; i < sizeof(orig); i++) {
        lp_transmit(&lp, orig[i]);
    }

    uint8_t rcvd[20];
    TEST_ASSERT_EQUAL(20, lp_receive(&lp, rcvd, sizeof(rcvd)));
    TEST_ASSERT_EQUAL_MEMORY(orig, rcvd, 20);
}
