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


DeadcomL2Result dcConnect(DeadcomL2 *deadcom) {
    if (deadcom == NULL) {
        return DC_FAILURE;
    }
    deadcom->t->mutexLock(deadcom->mutex_p);

    if (deadcom->state == DC_CONNECTED) {
        // no-op then
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_OK;
    } else if (deadcom->state == DC_CONNECTING) {
        // some other thread is attempting connection
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    }

    deadcom->state = DC_CONNECTING;

    // Construct a CONN frame and transmit it
    yahdlc_control_t control_connect = {
        .frame = YAHDLC_FRAME_CONN,
        .seq_no = 0
    };
    char frame_data[8];
    unsigned int frame_length;
    yahdlc_frame_data(&control_connect, NULL, 0, frame_data, &frame_length);
    deadcom->transmitBytes(frame_data, frame_length);

    // Wait on conditional variable. This condvar will be signaled once the other station has
    // acknowledged our connection request (or decided to initiate connection at the same time).
    bool result = deadcom->t->condvarWait(deadcom->condvar_p, DEADCOM_CONN_TIMEOUT_MS);

    if (!result) {
        // Wait on the conditional variable timed out, no response was received.
        deadcom->state = DC_DISCONNECTED;
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_NOT_CONNECTED;
    } else {
        // Connection successful, reset state variables and transition to connected state
        deadcom->send_number = 0;
        deadcom->last_acked = 0;
        deadcom->recv_number = 0;
        deadcom->failure_count = 0;
        deadcom->state = DC_CONNECTED;
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_OK;
    }
}
