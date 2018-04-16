/**
 * @file    dcl2-pthreads.h
 * @brief   DeadCom Layer 2 pthreads integration
 *
 * This is a small helper library which declares DeadcomL2ThreadingVMT structure that can be used
 * on systems which provide `pthreads` library.
 */

#ifndef __DEADCOML2_PTHREADS_H
#define __DEADCOML2_PTHREADS_H

#include <pthread.h>
#include "dcl2.h"


extern DeadcomL2ThreadingMethods pthreadsDeadcom;

/**
 * @brief A condvar-mutex combo
 *
 * dcl2 API expects condvarWait function which takes condvar only, pthread provides a function which
 * takes both condvar and mutex. This structure is used in place of mutex type to carry both.
 */

typedef struct {
    pthread_cond_t  *cond;
    pthread_mutex_t *mutx;
} dcl2_pthread_cond_t;


/**
 * Initialize an object representing a DeadCom link suitable for use with pthreads.
 *
 * This function is a wrapper around `dcInit`. It passes through all arguments to dcInit.
 * Additionally, it dynamically allocates objects representing a mutex and condvar suitable for
 * use with `pthreadsDeadcom` (threading VMT) defined by this library.
 *
 * All present params and return values are the same as `dcInit`
 */
DeadcomL2Result dcPthreadsInit(DeadcomL2 *deadcom, bool (*transmitBytes)(const uint8_t*, size_t));


/**
 * Free pthread objects in DeadCom link.
 *
 * This function deallocates all memory allocated by dcPthreadsInit on the given deadcom link.
 */
void dcPthreadsFree(DeadcomL2 *deadcom);


#endif
