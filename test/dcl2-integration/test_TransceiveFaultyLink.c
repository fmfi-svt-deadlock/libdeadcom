#define _GNU_SOURCE
#include "unity.h"
#include "fff.h"
#include "leaky-pipe.h"

#include "dcl2.h"
#include "dcl2-pthreads.h"

#include "common.h"


void test_SendMessageDropInMsg() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    unsigned int dl[] = {10, 12};
    args_c_tx.drop_list = dl;
    args_c_tx.drop_list_len = 2;

    run_1msg_test(&args_c_tx, &args_r_tx);
}


void test_SendMessageDropInAck() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    unsigned int dl[] = {10};
    args_r_tx.drop_list = dl;
    args_r_tx.drop_list_len = 1;

    run_1msg_test(&args_c_tx, &args_r_tx);
}


void test_Send1000MessagesOverDroppyTx() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    args_c_tx.drop_prob = 0.0005;

    run_1000msg_test(&args_c_tx, &args_r_tx);
}


void test_Send1000HugeMessagesOverDroppyRx() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    args_r_tx.drop_prob = 0.01; // Drop approximately every 50th byte

    run_1000msg_test(&args_c_tx, &args_r_tx);
}


void test_Send1000MessagesOverNoisyCorruptingDroppyTx() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    args_c_tx.drop_prob = 0.0002;
    args_c_tx.corrupt_prob = 0.0002;
    args_c_tx.add_prob = 0.0002;

    run_1000msg_test(&args_c_tx, &args_r_tx);
}


void test_Send1000HugeMessagesOverNoisyCorruptingDroppyRx() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    args_r_tx.drop_prob = 0.0005;
    args_r_tx.corrupt_prob = 0.0005;
    args_r_tx.add_prob = 0.0005;

    run_1000msg_test(&args_c_tx, &args_r_tx);
}
