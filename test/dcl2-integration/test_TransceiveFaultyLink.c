#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "unity.h"
#include "fff.h"
#include "leaky-pipe.h"

#include "dcl2.h"
#include "dcl2-pthreads.h"

#include "common.h"

DEFINE_FFF_GLOBALS;
FAKE_VOID_FUNC(dummyTransmitBytes, uint8_t*, uint8_t);


DeadcomL2 *dc, *dr;

void setUp(void) {
    RESET_FAKE(dummyTransmitBytes);
    for (int i = 0; i < TEST_THREADS; i++) {
        threads[i] = 0;
    }
    c_tx_pipe = malloc(sizeof(leaky_pipe_t));
    r_tx_pipe = malloc(sizeof(leaky_pipe_t));
    dc = malloc(sizeof(DeadcomL2));
    dr = malloc(sizeof(DeadcomL2));
}

void tearDown(void) {
    cutLinks();
    for (int i = 0; i < TEST_THREADS; i++) {
        if (threads[i] != 0) {
            pthread_cancel(threads[i]);
            void* retval;
            pthread_join(threads[i], &retval);
        }
    }
    free(c_tx_pipe);
    free(r_tx_pipe);
    free(dc);
    free(dr);
}

void* sender_1000msg_thread(void *p) {
    unsigned int r1 = 1;
    unsigned int conn_attempt;
    // multiple connection attempts, since we are working over lossy link
    for (conn_attempt = 3; conn_attempt > 0; conn_attempt--) {
        if (dcConnect(dc) == DC_OK) {
            break;
        }
    }
    THREADED_ASSERT(DC_OK == dcConnect(dc)); // No-op if connected, just a check. Disaster if not.
    for (unsigned int i = 0; i < 1000; i++) {
        uint8_t message[120];
        for (unsigned int j = 0; j < sizeof(message); j++) {
            message[j] = rand_r(&r1) % 256;
        }
        THREADED_ASSERT(DC_OK == dcSendMessage(dc, message, sizeof(message)));
        pthread_testcancel();
    }
    THREAD_EXIT_OK();
}

void* receiver_1000msg_thread(void *p) {
    unsigned int r2 = 1;
    for (unsigned int i = 0; i < 1000; i++) {
        int16_t msgLen;
        while (DC_E_NOMSG == (msgLen = dcGetReceivedMsgLen(dr))) {
            // Even though nanosleep should be a cancellation point according to POSIX,
            // on musl, attempting to cancel a nanosleep-ing and then join it will result in
            // join never returning (even though the sleeping loop stops executing).
            // I'm not sure what I'm doing wrong, however disabling cancelability around
            // nanosleep and explicitly calling testcancel seems to work so I'm leaving it.
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
            struct timespec t;
            t.tv_sec = 0;
            t.tv_nsec = 2000000;
            nanosleep(&t, &t);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            pthread_testcancel();
        }
        THREADED_ASSERT(120 == msgLen);
        uint8_t rcvdMessage[msgLen];
        THREADED_ASSERT(msgLen == dcGetReceivedMsg(dr, rcvdMessage));
        for (unsigned int j = 0; j < msgLen; j++) {
            THREADED_ASSERT((rand_r(&r2) % 256) == rcvdMessage[j]);
        }
        pthread_testcancel();
    }
    THREAD_EXIT_OK();
}

void run_1000msg_test(lp_args_t *args_c_tx, lp_args_t *args_r_tx) {
    createLinksAndReceiveThreads(args_c_tx, args_r_tx, dc, dr);
    declareAssertingThreads(2);

    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &sender_1000msg_thread, NULL));
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_R_TX], NULL, &receiver_1000msg_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1) * 1000;
    waitForThreadsAndAssert(timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc->state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr->state);
}

void* sender_1msg_thread(void *p) {
    unsigned int seed = 1;
    uint8_t message[10] = {0};
    for (unsigned int i = 0; i < sizeof(message); i++) {
        message[i] = rand_r(&seed) % 256;
    }

    unsigned int conn_attempt;
    // multiple connection attempts, since we are working over lossy link
    for (conn_attempt = 3; conn_attempt > 0; conn_attempt--) {
        if (dcConnect(dc) == DC_OK) {
            break;
        }
    }
    THREADED_ASSERT(DC_OK == dcConnect(dc)); // No-op if connected, just a check. Disaster if not.

    THREADED_ASSERT(DC_OK == dcSendMessage(dc, message, sizeof(message)));
    THREAD_EXIT_OK();
}

void* receiver_1msg_thread(void *p) {
    unsigned int seed = 1;
    uint8_t message[10] = {0};
    for (unsigned int i = 0; i < sizeof(message); i++) {
        message[i] = rand_r(&seed) % 256;
    }
    int16_t msgLen;
    while (DC_E_NOMSG == (msgLen = dcGetReceivedMsgLen(dr))) {
        usleep(2000);
    }
    THREADED_ASSERT(sizeof(message) == msgLen);
    uint8_t rcvdMessage[msgLen];
    THREADED_ASSERT(msgLen == dcGetReceivedMsg(dr, rcvdMessage));
    THREADED_ASSERT(memcmp(message, rcvdMessage, sizeof(message)) == 0);
    THREAD_EXIT_OK();
}

void run_1msg_test(lp_args_t *args_c_tx, lp_args_t *args_r_tx) {
    createLinksAndReceiveThreads(args_c_tx, args_r_tx, dc, dr);
    declareAssertingThreads(2);

    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_TX], NULL, &sender_1msg_thread, NULL));
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_R_TX], NULL, &receiver_1msg_thread, NULL));

    long timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    waitForThreadsAndAssert(timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc->state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr->state);
}



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
