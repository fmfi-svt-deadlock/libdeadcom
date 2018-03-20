#include <string.h>
#include "dcl2.h"


static void resetLink(DeadcomL2 *deadcom) {
    deadcom->send_number = 0;
    deadcom->last_acked = 0;
    deadcom->recv_number = 0;
    deadcom->failure_count = 0;
    deadcom->extractionBufferSize = DC_E_NOMSG;
    deadcom->extractionComplete = false;
    deadcom->state = DC_DISCONNECTED;
}


DeadcomL2Result dcInit(DeadcomL2 *deadcom, void *_mutex_p, void *_condvar_p,
                       DeadcomL2ThreadingVMT *_t, void (*transmitBytes)(uint8_t*, uint8_t)) {
    if (deadcom == NULL || transmitBytes == NULL || _mutex_p == NULL || _condvar_p == NULL ||
        _t == NULL) {
        return DC_FAILURE;
    }

    // Initialize DeadcomL2 structure
    memset(deadcom, 0, sizeof(DeadcomL2));
    resetLink(deadcom);
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
        .frame = YAHDLC_FRAME_CONN
    };
    // Calculate frame size
    unsigned int frame_length;
    yahdlc_frame_data(&control_connect, NULL, 0, NULL, &frame_length);

    // Create the frame
    char frame_data[frame_length];
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
        resetLink(deadcom);
        deadcom->state = DC_CONNECTED;
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_OK;
    }
}


DeadcomL2Result dcDisconnect(DeadcomL2 *deadcom) {
    if (deadcom == NULL) {
        return DC_FAILURE;
    }
    deadcom->t->mutexLock(deadcom->mutex_p);

    if (deadcom->state == DC_CONNECTING) {
        // Cant disconnect connecting link
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    }

    deadcom->state = DC_DISCONNECTED;
    deadcom->t->mutexUnlock(deadcom->mutex_p);
    return DC_OK;
}


DeadcomL2Result dcSendMessage(DeadcomL2 *deadcom,  const uint8_t *message,
                              const uint8_t message_len) {
    if (deadcom == NULL || message == NULL || message_len == 0 ||
        message_len > DEADCOM_PAYLOAD_MAX_LEN) {
        return DC_FAILURE;
    }
    deadcom->t->mutexLock(deadcom->mutex_p);

    if (deadcom->state == DC_TRANSMITTING) {
        // We are already awaiting reponse on a message, we can't transmit another
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    } else if (deadcom->state != DC_CONNECTED) {
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_NOT_CONNECTED;
    }

    yahdlc_control_t control = {
        .frame = YAHDLC_FRAME_DATA,
        .send_seq_no = deadcom->send_number,
        .recv_seq_no = deadcom->recv_number
    };
    deadcom->send_number = (deadcom->send_number + 1) % 7;
    deadcom->failure_count = 0;
    deadcom->state = DC_TRANSMITTING;

    unsigned int frame_len;
    if (yahdlc_frame_data(&control, message, message_len, NULL, &frame_len) != -EINVAL) {

        uint8_t frame[frame_len];
        yahdlc_frame_data(&control, message, message_len, frame, &frame_len);

        bool transmit_success = false;
        deadcom->failure_count = 0;
        while (deadcom->failure_count < DEADCOM_MAX_FAILURE_COUNT) {

            deadcom->transmitBytes(frame, frame_len);
            bool result = deadcom->t->condvarWait(deadcom->condvar_p, DEADCOM_ACK_TIMEOUT_MS);

            if (result) {
                // No timeout
                if (deadcom->last_response == DC_RESP_OK) {
                    transmit_success = true;
                    break;
                } else if (deadcom->last_response == DC_RESP_NOLINK) {
                    // The other station dropped link. Let's drop ours
                    transmit_success = false;
                    break;
                } /* else it must have been DC_RESP_REJECT and in this case we want the loop to
                continue normally */
            }
            deadcom->failure_count++;
        }

        if (transmit_success) {
            // last_acked number was updated by receive thread when handling DC_ACK response
            deadcom->state = DC_CONNECTED;
            deadcom->t->mutexUnlock(deadcom->mutex_p);
            return DC_OK;
        } else {
            // the other station is unresponsive, reset the link.
            resetLink(deadcom);
            deadcom->t->mutexUnlock(deadcom->mutex_p);
            return DC_LINK_RESET;
        }

    } else {
        // Invalid parameters passwd to frame_data function
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    }
}


int16_t dcGetReceivedMsgLen(DeadcomL2 *deadcom) {
    if (deadcom == NULL) {
        return DC_E_FAIL;
    }

    // Lock and unlock mutex so that we wait if someone is modifying deadcom struct
    deadcom->t->mutexLock(deadcom->mutex_p);
    deadcom->t->mutexUnlock(deadcom->mutex_p);
    if (deadcom->extractionComplete) {
        return deadcom->extractionBufferSize;
    } else {
        return DC_E_NOMSG;
    }
}


int16_t dcGetReceivedMsg(DeadcomL2 *deadcom, uint8_t *buffer) {
    if (deadcom == NULL || buffer == NULL) {
        return DC_E_FAIL;
    }

    deadcom->t->mutexLock(deadcom->mutex_p);

    if (!deadcom->extractionComplete) {
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_E_NOMSG;
    }

    memcpy(buffer, deadcom->extractionBuffer, deadcom->extractionBufferSize);

    // acknowledge reception and frame processing
    yahdlc_control_t control_ack = {
        .frame = YAHDLC_FRAME_ACK,
        .recv_seq_no = deadcom->recv_number
    };

    unsigned int ack_frame_length;
    yahdlc_frame_data(&control_ack, NULL, 0, NULL, &ack_frame_length);
    
    uint8_t ack_frame[ack_frame_length];
    yahdlc_frame_data(&control_ack, NULL, 0, ack_frame, &ack_frame_length);
    deadcom->transmitBytes(ack_frame, ack_frame_length);

    int16_t copied_size = deadcom->extractionBufferSize;
    deadcom->extractionBufferSize = DC_E_NOMSG;

    deadcom->t->mutexUnlock(deadcom->mutex_p);
    return copied_size;
}
