#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include "unity.h"
#include "fff.h"
#include "leaky-pipe.h"

#include "dcl2.h"
#include "dcl2-pthreads.h"

#include "common.h"

#define UNUSED_PARAM(x)  (void)(x);

DEFINE_FFF_GLOBALS;
FAKE_VALUE_FUNC(bool, dummyTransmitBytes, const uint8_t*, size_t);


void test_InitializeDCL2Pthreads() {
    RESET_FAKE(dummyTransmitBytes);
    dummyTransmitBytes_fake.return_val = true;
    DeadcomL2 d;
    TEST_ASSERT_EQUAL(DC_OK, dcPthreadsInit(&d, &dummyTransmitBytes));
}


void* try_connect_thread(void* p) {
    UNUSED_PARAM(p);
    dcConnect(dc);
    dcConnect(dc);
    THREAD_EXIT_OK();
}

void test_ConnectNoLink() {
    lp_args_t args;
    lp_init_args(&args);
    args.drop_prob = 1; // A dead link basically

    createLinksAndReceiveThreads(&args, &args, dc, dr);
    declareAssertingThreads(1);

    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &try_connect_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    waitForThreadsAndAssert(timeout);
    cutLinksAndJoinReceiveThreads();
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, dc->state);
}

void test_ConnectFlawlessLink() {
    lp_args_t args;
    lp_init_args(&args);

    createLinksAndReceiveThreads(&args, &args, dc, dr);
    declareAssertingThreads(1);

    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &try_connect_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    waitForThreadsAndAssert(timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc->state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr->state);
}

void test_ConnectDropInRequest() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    unsigned int dl[] = {2};
    args_c_tx.drop_list = dl;
    args_c_tx.drop_list_len = 1;

    createLinksAndReceiveThreads(&args_c_tx, &args_r_tx, dc, dr);
    declareAssertingThreads(1);

    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &try_connect_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    waitForThreadsAndAssert(timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc->state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr->state);
}

void test_ConnectDropInResponse() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    unsigned int dl[] = {2};
    args_r_tx.drop_list = dl;
    args_r_tx.drop_list_len = 1;

    createLinksAndReceiveThreads(&args_c_tx, &args_r_tx, dc, dr);
    declareAssertingThreads(1);

    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &try_connect_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    waitForThreadsAndAssert(timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc->state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr->state);
}
