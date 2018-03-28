#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include "unity.h"
#include "fff.h"

#include "dcl2.h"
#include "dcl2-pthreads.h"

DEFINE_FFF_GLOBALS;
FAKE_VOID_FUNC(dummyTransmitBytes, uint8_t*, uint8_t);

/*
 * Integration tests of Deadcom Layer 2 library. Threading is implemented using pthreads helper
 */

#define TEST_THREADS  4
pthread_t threads[TEST_THREADS];

void setUp(void) {
    RESET_FAKE(dummyTransmitBytes);
    for (int i = 0; i < TEST_THREADS; i++) {
        threads[i] = 0;
    }
}

void tearDown(void) {
    for (int i = 0; i < TEST_THREADS; i++) {
        if (threads[i] != 0) {
            pthread_cancel(threads[i]);
            void* retval;
            pthread_join(threads[i], &retval);
        }
    }
}


void test_InitializeDCL2Pthreads() {
    DeadcomL2 d;
    TEST_ASSERT_EQUAL(DC_OK, dcPthreadsInit(&d, &dummyTransmitBytes));
}


void test_ConnectNoLink() {
    DeadcomL2 dc;
    TEST_ASSERT_EQUAL(DC_OK, dcPthreadsInit(&dc, &dummyTransmitBytes));

    void* run_connect(void* p) {
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        TEST_ASSERT_EQUAL(DC_NOT_CONNECTED, dcConnect(&dc));
    }

    TEST_ASSERT_EQUAL(0, pthread_create(&threads[0], NULL, &run_connect, NULL));

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += (DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1)) * 1000000;
    ts.tv_sec  += (ts.tv_nsec / 1000000000);
    ts.tv_nsec %= 1000000000;

    void *retval;
    int ret = pthread_timedjoin_np(threads[0], &retval, &ts);
    if (ret != ETIMEDOUT) {
        // The thread was joined, therefore it is not valid any more
        threads[0] = 0;
    }
    TEST_ASSERT_NOT_EQUAL(ETIMEDOUT, ret);
}
