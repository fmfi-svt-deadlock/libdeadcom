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

/**************************************************************************************************/
/* Misc helpers */

pthread_mutex_t  test_mtx;
pthread_cond_t   test_cnd;
volatile int     test_assert_status;

DeadcomL2 *dc, *dr;

int pthread_reltimedjoin(pthread_t thread, void **retval, long milliseconds) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += milliseconds * 1000000;
    ts.tv_sec  += (ts.tv_nsec / 1000000000);
    ts.tv_nsec %= 1000000000;
    return pthread_timedjoin_np(thread, retval, &ts);
}

void pthread_reltimedjoin_assert_notimeout(pthread_t *thread, long milliseconds) {
    void *retval;
    int ret = pthread_reltimedjoin(*thread, &retval, milliseconds);
    if (ret != ETIMEDOUT) {
        // The thread was joined, therefore it is not valid any more
        *thread = 0;
    }
    TEST_ASSERT_NOT_EQUAL(ETIMEDOUT, ret);
}

void declareAssertingThreads(unsigned int n) {
    test_assert_status = -1 * ((signed int)n);
}

void waitForThreadsAndAssert(long milliseconds) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += milliseconds * 1000000;
    ts.tv_sec  += (ts.tv_nsec / 1000000000);
    ts.tv_nsec %= 1000000000;

    pthread_mutex_lock(&test_mtx);
    int timedwait_retcode = 0;
    while (0 > test_assert_status && timedwait_retcode != ETIMEDOUT) {
        timedwait_retcode = pthread_cond_timedwait(&test_cnd, &test_mtx, &ts);
    }
    pthread_mutex_unlock(&test_mtx);
    if (timedwait_retcode == ETIMEDOUT) {
        char *failStr = "Threads under test did not finish in time\0";
        TEST_FAIL_MESSAGE(failStr);
    }
    if (test_assert_status != 0) {
        char failStr[100];
        memset(failStr, '\0', sizeof(failStr));
        sprintf(failStr, "Assert failed in tested thread on line %d", test_assert_status);
        TEST_FAIL_MESSAGE(failStr);
    }
}

/* End misc helpers */
/**************************************************************************************************/


/**************************************************************************************************/
/* Link emulation and data processing thread helpers */

pthread_t threads[TEST_THREADS];
leaky_pipe_t *c_tx_pipe;
leaky_pipe_t *r_tx_pipe;

typedef struct {
    DeadcomL2 *station;
    leaky_pipe_t *rx_pipe;
    char station_char;
} rx_set_t;

void* rx_handle_thread(void *p) {
    rx_set_t *rp = (rx_set_t*) p;
    uint8_t b[1];
    while (lp_receive(rp->rx_pipe, b, 1)) {
        // Uncomment this if you want to see bytes exchanged between simulated stations
        // printf("Station %c received %02x\n", rp->station_char, b[0]);
        dcProcessData(rp->station, b, 1);
        pthread_testcancel();
    }
}

void station_c_tx(uint8_t *bytes, uint8_t b_l) {
    for (unsigned int i = 0; i < b_l; i++) {
        lp_transmit(c_tx_pipe, bytes[i]);
    }
}

void station_r_tx(uint8_t *bytes, uint8_t b_l) {
    for (unsigned int i = 0; i < b_l; i++) {
        lp_transmit(r_tx_pipe, bytes[i]);
    }
}

void createLinksAndReceiveThreads(lp_args_t *c_tx_args, lp_args_t *r_tx_args, DeadcomL2 *station_c,
                                  DeadcomL2 *station_r) {
    lp_init(c_tx_pipe, c_tx_args);
    lp_init(r_tx_pipe, r_tx_args);

    TEST_ASSERT_EQUAL(DC_OK, dcPthreadsInit(station_c, &station_c_tx));
    TEST_ASSERT_EQUAL(DC_OK, dcPthreadsInit(station_r, &station_r_tx));

    rx_set_t *c = malloc(sizeof(rx_set_t)), *r = malloc(sizeof(rx_set_t));
    c->station = station_c; c->rx_pipe = r_tx_pipe; c->station_char = 'C';
    r->station = station_r; r->rx_pipe = c_tx_pipe; r->station_char = 'R';

    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_C_RX], NULL, &rx_handle_thread, c));
    TEST_ASSERT_EQUAL(0, pthread_create(&threads[STATION_R_RX], NULL, &rx_handle_thread, r));
}

void cutLinksAndJoinReceiveThreads() {
    void *retval;
    int ret;

    lp_cutoff(c_tx_pipe);
    ret = pthread_reltimedjoin(threads[STATION_R_RX], &retval,
                               DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT * 20));
    if (ret != ETIMEDOUT) {
        // The thread was joined, therefore it is not valid any more
        threads[STATION_R_RX] = 0;
    }
    TEST_ASSERT_NOT_EQUAL(ETIMEDOUT, ret);

    lp_cutoff(r_tx_pipe);
    ret = pthread_reltimedjoin(threads[STATION_C_RX], &retval,
                               DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT * 20));
    if (ret != ETIMEDOUT) {
        // The thread was joined, therefore it is not valid any more
        threads[STATION_C_RX] = 0;
    }
    TEST_ASSERT_NOT_EQUAL(ETIMEDOUT, ret);
}

void cutLinks() {
    lp_cutoff(c_tx_pipe);
    lp_cutoff(r_tx_pipe);
}

/* End link emulation and data processing thread helpers */
/**************************************************************************************************/


/**************************************************************************************************/
/* Common test setup and teardown */

void setUp(void) {
    for (int i = 0; i < TEST_THREADS; i++) {
        threads[i] = 0;
    }
    c_tx_pipe = malloc(sizeof(leaky_pipe_t));
    memset(c_tx_pipe, 0, sizeof(leaky_pipe_t));
    r_tx_pipe = malloc(sizeof(leaky_pipe_t));
    memset(r_tx_pipe, 0, sizeof(leaky_pipe_t));
    dc = malloc(sizeof(DeadcomL2));
    dr = malloc(sizeof(DeadcomL2));
}

void tearDown(void) {
    unsigned int f;
    cutLinks();
    sleep(1);
    for (int i = 0; i < TEST_THREADS; i++) {
        if (threads[i] != 0) {
            pthread_cancel(threads[i]);
            sleep(1);
            void* retval;
            pthread_join(threads[i], &retval);
        }
    }
    free(c_tx_pipe);
    free(r_tx_pipe);
    free(dc);
    free(dr);
}

/* End common test setup and teardown */
/**************************************************************************************************/


/**************************************************************************************************/
/* Common test threads and elements */

static void* sender_1000msg_thread(void *p) {
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

static void* receiver_1000msg_thread(void *p) {
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

static void* sender_1msg_thread(void *p) {
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

static void* receiver_1msg_thread(void *p) {
    unsigned int seed = 1;
    uint8_t message[10] = {0};
    for (unsigned int i = 0; i < sizeof(message); i++) {
        message[i] = rand_r(&seed) % 256;
    }
    int16_t msgLen;
    while (DC_E_NOMSG == (msgLen = dcGetReceivedMsgLen(dr))) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        struct timespec t;
        t.tv_sec = 0;
        t.tv_nsec = 2000000;
        nanosleep(&t, &t);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_testcancel();
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

/* End test threads and elements */
/**************************************************************************************************/
