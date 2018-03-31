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

DEFINE_FFF_GLOBALS;
FAKE_VOID_FUNC(dummyTransmitBytes, uint8_t*, uint8_t);


/**************************************************************************************************/
/* Misc helpers */

int pthread_reltimedjoin(pthread_t thread, void **retval, unsigned int milliseconds) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += milliseconds * 1000000;
    ts.tv_sec  += (ts.tv_nsec / 1000000000);
    ts.tv_nsec %= 1000000000;
    return pthread_timedjoin_np(thread, retval, &ts);
}

void pthread_reltimedjoin_assert_notimeout(pthread_t *thread, unsigned int milliseconds) {
    void *retval;
    int ret = pthread_reltimedjoin(*thread, &retval, milliseconds);
    if (ret != ETIMEDOUT) {
        // The thread was joined, therefore it is not valid any more
        *thread = 0;
    }
    TEST_ASSERT_NOT_EQUAL(ETIMEDOUT, ret);
}

/* End misc helpers */
/**************************************************************************************************/


/**************************************************************************************************/
/* Link emulation and data processing thread helpers */

#define TEST_THREADS  4
pthread_t threads[TEST_THREADS];
leaky_pipe_t *c_tx_pipe;
leaky_pipe_t *r_tx_pipe;

#define STATION_C_TX  0
#define STATION_C_RX  1
#define STATION_R_TX  2
#define STATION_R_RX  3


typedef struct {
    DeadcomL2 *station;
    leaky_pipe_t *rx_pipe;
    char station_char;
} rx_set_t;

void* rx_handle_thread(void *p) {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    rx_set_t *rp = (rx_set_t*) p;
    uint8_t b[1];
    while (lp_receive(rp->rx_pipe, b, 1)) {
        if (TEST_DBG) {
            printf("Station %c received: %02x\n", rp->station_char, b[0]);
        }
        dcProcessData(rp->station, b, 1);
    }
}

void station_c_tx(uint8_t *bytes, uint8_t b_l) {
    for (unsigned int i = 0; i < b_l; i++) {
        if (TEST_DBG) {
            printf("Station C transmitted: %02x\n", bytes[i]);
        }
        lp_transmit(c_tx_pipe, bytes[i]);
    }
}

void station_r_tx(uint8_t *bytes, uint8_t b_l) {
    for (unsigned int i = 0; i < b_l; i++) {
        if (TEST_DBG) {
            printf("Station R transmitted: %02x\n", bytes[i]);
        }
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
                               DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT * 2));
    if (ret != ETIMEDOUT) {
        // The thread was joined, therefore it is not valid any more
        threads[STATION_R_RX] = 0;
    }
    TEST_ASSERT_NOT_EQUAL(ETIMEDOUT, ret);

    lp_cutoff(r_tx_pipe);
    ret = pthread_reltimedjoin(threads[STATION_C_RX], &retval,
                               DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT * 2));
    if (ret != ETIMEDOUT) {
        // The thread was joined, therefore it is not valid any more
        threads[STATION_C_RX] = 0;
    }
    TEST_ASSERT_NOT_EQUAL(ETIMEDOUT, ret);
}

/* End link emulation and data processing thread helpers */
/**************************************************************************************************/


/**************************************************************************************************/
/* Integration tests of Deadcom Layer 2 library. Threading is implemented using pthreads helper */

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

    unsigned int timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
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

    unsigned int timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
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

    unsigned int timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
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

    unsigned int timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
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

    unsigned int timeout = DEADCOM_CONN_TIMEOUT_MS * (DEADCOM_MAX_FAILURE_COUNT + 1);
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_C_TX], timeout);
    pthread_reltimedjoin_assert_notimeout(&threads[STATION_R_TX], timeout);
    cutLinksAndJoinReceiveThreads();

    TEST_ASSERT_EQUAL(DC_CONNECTED, dc.state);
    TEST_ASSERT_EQUAL(DC_CONNECTED, dr.state);
}
