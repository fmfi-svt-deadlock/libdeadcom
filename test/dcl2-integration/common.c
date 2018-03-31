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
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    rx_set_t *rp = (rx_set_t*) p;
    uint8_t b[1];
    while (lp_receive(rp->rx_pipe, b, 1)) {
        dcProcessData(rp->station, b, 1);
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
