#include "unity.h"
#include "fff.h"
#include "leaky-pipe.h"

#include "dcl2.h"
#include "dcl2-pthreads.h"

#include "common.h"


void test_SendMessage() {
    lp_args_t args;
    lp_init_args(&args);

    run_1msg_test(&args, &args);
}


void test_Send1000HugeMessagesUnidirectional() {
    lp_args_t args;
    lp_init_args(&args);

    run_1000msg_test(&args, &args);
}
