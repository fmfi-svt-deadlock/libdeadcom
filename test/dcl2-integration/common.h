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


int pthread_reltimedjoin(pthread_t thread, void **retval, long milliseconds);
void pthread_reltimedjoin_assert_notimeout(pthread_t *thread, long milliseconds);

void createLinksAndReceiveThreads(lp_args_t *c_tx_args, lp_args_t *r_tx_args, DeadcomL2 *station_c,
                                  DeadcomL2 *station_r);
void cutLinksAndJoinReceiveThreads();

#endif
