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


void test_SendMessage() {
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


void test_Send1000HugeMessagesUnidirectional() {
    lp_args_t args;
    lp_init_args(&args);

    DeadcomL2 dc;
    DeadcomL2 dr;
    createLinksAndReceiveThreads(&args, &args, &dc, &dr);

    unsigned int r1 = 1, r2 = 1;
    unsigned int messages_to_send = 1000;

    void* controller_thread(void *p) {
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        TEST_ASSERT_EQUAL(DC_OK, dcConnect(&dc));
        for (unsigned int i = 0; i < messages_to_send; i++) {
            uint8_t message[120];
            for (unsigned int j = 0; j < sizeof(message); j++) {
                message[j] = rand_r(&r1) % 256;
            }
            TEST_ASSERT_EQUAL(DC_OK, dcSendMessage(&dc, message, sizeof(message)));
        }
    }
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &controller_thread, NULL));

    void* reader_thread(void *p) {
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        for (unsigned int i = 0; i < messages_to_send; i++) {
            int16_t msgLen;
            while (DC_E_NOMSG == (msgLen = dcGetReceivedMsgLen(&dr))) {
                struct timespec t1, t2;
                t1.tv_sec = 0;
                t1.tv_nsec = 2000000;
                nanosleep(&t1, &t2);
            }
            TEST_ASSERT_EQUAL(120, msgLen);
            uint8_t rcvdMessage[msgLen];
            TEST_ASSERT_EQUAL(msgLen, dcGetReceivedMsg(&dr, rcvdMessage));
            for (unsigned int j = 0; j < msgLen; j++) {
                TEST_ASSERT_EQUAL((rand_r(&r2) % 256), rcvdMessage[j]);
            }
        }
    }
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_R_TX], NULL, &reader_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1) * messages_to_send;
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_C_TX], timeout);
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_R_TX], timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc.state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr.state);
}
