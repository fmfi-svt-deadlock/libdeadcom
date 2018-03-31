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

#define TEST_DBG  false

#include "common.h"

DEFINE_FFF_GLOBALS;
FAKE_VOID_FUNC(dummyTransmitBytes, uint8_t*, uint8_t);


void setUp(void) {
    RESET_FAKE(dummyTransmitBytes);
    for (int i = 0; i < TEST_THREADS; i++) {
        threads[i] = 0;
    }
    c_tx_pipe = malloc(sizeof(leaky_pipe_t));
    r_tx_pipe = malloc(sizeof(leaky_pipe_t));
}

void tearDown(void) {
    for (int i = 0; i < TEST_THREADS; i++) {
        if (threads[i] != 0) {
            pthread_cancel(threads[i]);
            void* retval;
            pthread_join(threads[i], &retval);
        }
    }
    free(c_tx_pipe);
    free(r_tx_pipe);
}


void test_InitializeDCL2Pthreads() {
    DeadcomL2 d;
    TEST_ASSERT_EQUAL(DC_OK, dcPthreadsInit(&d, &dummyTransmitBytes));
}

void test_ConnectNoLink() {
    lp_args_t args;
    lp_init_args(&args);
    args.drop_prob = 1; // A dead link basically

    DeadcomL2 dc;
    DeadcomL2 dr;
    createLinksAndReceiveThreads(&args, &args, &dc, &dr);

    void* controller_thread(void* p) {
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        TEST_ASSERT_EQUAL(DC_NOT_CONNECTED, dcConnect(&dc));
    }
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &controller_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_C_TX], timeout);
    cutLinksAndJoinReceiveThreads();
}

void test_ConnectFlawlessLink() {
    lp_args_t args;
    lp_init_args(&args);

    DeadcomL2 dc;
    DeadcomL2 dr;
    createLinksAndReceiveThreads(&args, &args, &dc, &dr);

    void* controller_thread(void *p) {
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        TEST_ASSERT_EQUAL(DC_OK, dcConnect(&dc));
    }
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &controller_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_C_TX], timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc.state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr.state);
}

void test_ConnectDropInRequest() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    unsigned int dl[] = {2};
    args_c_tx.drop_list = dl;
    args_c_tx.drop_list_len = 1;

    DeadcomL2 dc;
    DeadcomL2 dr;
    createLinksAndReceiveThreads(&args_c_tx, &args_r_tx, &dc, &dr);

    void* controller_thread(void *p) {
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        TEST_ASSERT_EQUAL(DC_NOT_CONNECTED, dcConnect(&dc));
        TEST_ASSERT_EQUAL(DC_OK, dcConnect(&dc));
    }
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &controller_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_C_TX], timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc.state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr.state);
}

void test_ConnectDropInResponse() {
    lp_args_t args_c_tx, args_r_tx;
    lp_init_args(&args_c_tx);
    lp_init_args(&args_r_tx);
    unsigned int dl[] = {2};
    args_r_tx.drop_list = dl;
    args_r_tx.drop_list_len = 1;

    DeadcomL2 dc;
    DeadcomL2 dr;
    createLinksAndReceiveThreads(&args_c_tx, &args_r_tx, &dc, &dr);

    void* controller_thread(void *p) {
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        TEST_ASSERT_EQUAL(DC_NOT_CONNECTED, dcConnect(&dc));
        TEST_ASSERT_EQUAL(DC_OK, dcConnect(&dc));
    }
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &controller_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_C_TX], timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc.state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr.state);
}

void test_SendDataFlawlessLink() {
    lp_args_t args;
    lp_init_args(&args);

    DeadcomL2 dc;
    DeadcomL2 dr;
    createLinksAndReceiveThreads(&args, &args, &dc, &dr);

    uint8_t message[30] = {0};
    for (unsigned int i = 0; i < sizeof(message); i++) {
        message[i] = rand() % 256;
    }

    void* controller_thread(void *p) {
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        TEST_ASSERT_EQUAL(DC_OK, dcConnect(&dc));
        TEST_ASSERT_EQUAL(DC_OK, dcSendMessage(&dc, message, sizeof(message)));
    }
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &controller_thread, NULL));

    void* reader_thread(void *p) {
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        int16_t msgLen;
        while (DC_E_NOMSG == (msgLen = dcGetReceivedMsgLen(&dr))) {
            struct timespec t1, t2;
            t1.tv_sec = 0;
            t1.tv_nsec = 2000000;
            nanosleep(&t1, &t2);
        }
        TEST_ASSERT_EQUAL(sizeof(message), msgLen);
        uint8_t rcvdMessage[msgLen];
        TEST_ASSERT_EQUAL(msgLen, dcGetReceivedMsg(&dr, rcvdMessage));
        TEST_ASSERT_EQUAL_MEMORY(message, rcvdMessage, sizeof(message));
    }
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_R_TX], NULL, &reader_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_C_TX], timeout);
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_R_TX], timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc.state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr.state);
}
