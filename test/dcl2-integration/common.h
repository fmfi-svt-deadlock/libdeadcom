#ifndef __TEST_COMMON_H
#define __TEST_COMMON_H

#define TEST_THREADS  4

#define STATION_C_TX  0
#define STATION_C_RX  1
#define STATION_R_TX  2
#define STATION_R_RX  3


extern pthread_t threads[TEST_THREADS];
extern leaky_pipe_t *c_tx_pipe;
extern leaky_pipe_t *r_tx_pipe;

extern pthread_mutex_t  test_mtx;
extern pthread_cond_t   test_cnd;
extern volatile int     test_assert_status;

extern DeadcomL2 *dc, *dr;

#define THREADED_ASSERT(condition) if (!(condition)) { \
    pthread_mutex_lock(&test_mtx); \
    test_assert_status = __LINE__; \
    pthread_cond_signal(&test_cnd); \
    pthread_mutex_unlock(&test_mtx); \
    return NULL; }

#define THREAD_EXIT_OK() { \
    pthread_mutex_lock(&test_mtx); \
    test_assert_status += 1; \
    pthread_cond_signal(&test_cnd); \
    pthread_mutex_unlock(&test_mtx); \
    return NULL; }


void declareAssertingThreads(unsigned int n);
void waitForThreadsAndAssert(long milliseconds);
int pthread_reltimedjoin(pthread_t thread, void **retval, long milliseconds);
void pthread_reltimedjoin_assert_notimeout(pthread_t *thread, long milliseconds);

void createLinksAndReceiveThreads(lp_args_t *c_tx_args, lp_args_t *r_tx_args, DeadcomL2 *station_c,
                                  DeadcomL2 *station_r);
void cutLinksAndJoinReceiveThreads();
void cutLinks();

void setUp();
void tearDown();

void run_1000msg_test(lp_args_t *args_c_tx, lp_args_t *args_r_tx);
void run_1msg_test(lp_args_t *args_c_tx, lp_args_t *args_r_tx);

#endif
