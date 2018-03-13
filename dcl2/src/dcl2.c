#include <string.h>
#include "dcl2.h"


DeadcomL2Result dcInit(DeadcomL2 *deadcom, void *_mutex_p, void *_condvar_p,
                       DeadcomL2ThreadingVMT *_t, void (*transmitBytes)(uint8_t*, uint8_t)) {
    if (deadcom == NULL || transmitBytes == NULL || _mutex_p == NULL || _condvar_p == NULL ||
        _t == NULL) {
        return DC_FAILURE;
    }

    // Initialize DeadcomL2 structure
    memset(deadcom, 0, sizeof(DeadcomL2));
    deadcom->state = DC_DISCONNECTED;
    deadcom->transmitBytes = transmitBytes;
    deadcom->mutex_p = _mutex_p;
    deadcom->condvar_p = _condvar_p;
    deadcom->t = _t;

    // Initialize synchronization objects
    deadcom->t->mutexInit(deadcom->mutex_p);
    deadcom->t->condvarInit(deadcom->condvar_p);

    return DC_OK;
}
