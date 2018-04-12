#include <sys/time.h>
#include <stdlib.h>
#include "dcl2-pthreads.h"


bool dcl_pthreads_mutexInit(void *mutex_p) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init((pthread_mutex_t*) mutex_p, &attr);
    return true;
}


bool dcl_pthreads_mutexLock(void *mutex_p) {
    pthread_mutex_lock((pthread_mutex_t*) mutex_p);
    return true;
}


bool dcl_pthreads_mutexUnlock(void *mutex_p) {
    pthread_mutex_unlock((pthread_mutex_t*) mutex_p);
    return true;
}


bool dcl_pthreads_condvarInit(void *condvar_p) {
    dcl2_pthread_cond_t *c = (dcl2_pthread_cond_t*) condvar_p;
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(c->cond, &attr);
    return true;
}


bool dcl_pthreads_condvarWait(void *condvar_p, uint32_t milliseconds, bool *timed_out) {
    dcl2_pthread_cond_t *c = (dcl2_pthread_cond_t*) condvar_p;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec  += milliseconds / 1000;
    ts.tv_nsec += ((milliseconds % 1000) * 1000000);
    ts.tv_sec  += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;

    int ret = pthread_cond_timedwait(c->cond, c->mutx, &ts);
    *timed_out = (ret == ETIMEDOUT);
    return true;
}


bool dcl_pthreads_condvarSignal(void *condvar_p) {
    dcl2_pthread_cond_t *c = (dcl2_pthread_cond_t*) condvar_p;
    pthread_cond_signal(c->cond);
    return true;
}


DeadcomL2ThreadingMethods pthreadsDeadcom = {
    .mutexInit     = &dcl_pthreads_mutexInit,
    .mutexLock     = &dcl_pthreads_mutexLock,
    .mutexUnlock   = &dcl_pthreads_mutexUnlock,
    .condvarInit   = &dcl_pthreads_condvarInit,
    .condvarWait   = &dcl_pthreads_condvarWait,
    .condvarSignal = &dcl_pthreads_condvarSignal
};


DeadcomL2Result dcPthreadsInit(DeadcomL2 *deadcom,
                               bool (*transmitBytes)(const uint8_t*, size_t, void*),
                               void *transmissionContext) {
    pthread_mutex_t *mutx = malloc(sizeof(pthread_mutex_t));
    pthread_cond_t  *cond = malloc(sizeof(pthread_cond_t));
    dcl2_pthread_cond_t *combined_cond = malloc(sizeof(dcl2_pthread_cond_t));

    combined_cond->mutx = mutx;
    combined_cond->cond = cond;

    return dcInit(deadcom, mutx, combined_cond, &pthreadsDeadcom, transmitBytes,
                  transmissionContext);
}


void dcPthreadsFree(DeadcomL2 *deadcom) {
    dcl2_pthread_cond_t *combined_cond = deadcom->condvar_p;
    free(combined_cond->mutx);
    free(combined_cond->cond);
    free(combined_cond);
}
