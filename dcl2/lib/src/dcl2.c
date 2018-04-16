#include <string.h>
#include "dcl2.h"


static void resetLink(DeadcomL2 *deadcom) {
    deadcom->send_number = 0;
    deadcom->next_expected_ack = 0;
    deadcom->recv_number = 0;
    deadcom->failure_count = 0;
    deadcom->extractionBufferSize = 0;
    deadcom->extractionComplete = false;
    deadcom->state = DC_DISCONNECTED;
}


DeadcomL2Result dcInit(DeadcomL2 *deadcom, void *_mutex_p, void *_condvar_p,
                       DeadcomL2ThreadingMethods *_t,
                       bool (*transmitBytes)(const uint8_t*, size_t)) {
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
    if (!deadcom->t->mutexInit(deadcom->mutex_p)) {return DC_FAILURE;}
    if (!deadcom->t->condvarInit(deadcom->condvar_p)) {return DC_FAILURE;}

    return DC_OK;
}


DeadcomL2Result dcConnect(DeadcomL2 *deadcom) {
    if (deadcom == NULL) {
        return DC_FAILURE;
    }
    if (!deadcom->t->mutexLock(deadcom->mutex_p)) {return DC_FAILURE;}

    if (deadcom->state == DC_CONNECTED) {
        // no-op then
        if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {return DC_FAILURE;}
        return DC_OK;
    } else if (deadcom->state == DC_CONNECTING) {
        // some other thread is attempting connection
        // ext method retcode does not matter, we are signaling failure either way
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    }

    deadcom->state = DC_CONNECTING;

    // Construct a CONN frame and transmit it
    yahdlc_control_t control_connect = {
        .frame = YAHDLC_FRAME_CONN
    };
    // Calculate frame size
    size_t frame_length = 0;
    yahdlc_frame_data(&control_connect, NULL, 0, NULL, &frame_length);

    // Create the frame
    uint8_t frame_data[frame_length];
    yahdlc_frame_data(&control_connect, NULL, 0, frame_data, &frame_length);

    if (!deadcom->transmitBytes(frame_data, frame_length)) {
        deadcom->state = DC_DISCONNECTED;
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    }

    // Wait on conditional variable. This condvar will be signaled once the other station has
    // acknowledged our connection request (or decided to initiate connection at the same time).
    bool timed_out = false;
    if (!deadcom->t->condvarWait(deadcom->condvar_p, DEADCOM_CONN_TIMEOUT_MS, &timed_out)) {
        deadcom->state = DC_DISCONNECTED;
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    }

    if (timed_out) {
        // no response was received.
        deadcom->state = DC_DISCONNECTED;
        if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {return DC_FAILURE;}
        return DC_NOT_CONNECTED;
    } else {
        // Connection successful, reset state variables and transition to connected state
        resetLink(deadcom);
        deadcom->state = DC_CONNECTED;
        if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {
            deadcom->state = DC_DISCONNECTED;
            return DC_FAILURE;
        }
        return DC_OK;
    }
}


DeadcomL2Result dcDisconnect(DeadcomL2 *deadcom) {
    if (deadcom == NULL) {
        return DC_FAILURE;
    }
    if (!deadcom->t->mutexLock(deadcom->mutex_p)) {return DC_FAILURE;}

    if (deadcom->state == DC_CONNECTING) {
        // Cant disconnect connecting link
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    }

    deadcom->state = DC_DISCONNECTED;
    if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {return DC_FAILURE;}
    return DC_OK;
}


DeadcomL2Result dcSendMessage(DeadcomL2 *deadcom, const uint8_t *message, size_t message_len) {
    if (deadcom == NULL || message == NULL || message_len == 0 ||
        message_len > DEADCOM_PAYLOAD_MAX_LEN) {
        return DC_FAILURE;
    }
    if (!deadcom->t->mutexLock(deadcom->mutex_p)) {return DC_FAILURE;}

    if (deadcom->state == DC_TRANSMITTING) {
        // We are already awaiting reponse on a message, we can't transmit another
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    } else if (deadcom->state != DC_CONNECTED) {
        if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {return DC_FAILURE;}
        return DC_NOT_CONNECTED;
    }

    yahdlc_control_t control = {
        .frame = YAHDLC_FRAME_DATA,
        .send_seq_no = deadcom->send_number,
        .recv_seq_no = deadcom->recv_number
    };
    deadcom->send_number = (deadcom->send_number + 1) % 8;
    deadcom->failure_count = 0;
    deadcom->state = DC_TRANSMITTING;

    size_t frame_len;
    if (yahdlc_frame_data(&control, message, message_len, NULL, &frame_len) != -EINVAL) {

        uint8_t frame[frame_len];
        yahdlc_frame_data(&control, message, message_len, frame, &frame_len);

        bool transmit_success = false;
        deadcom->failure_count = 0;
        while (deadcom->failure_count < DEADCOM_MAX_FAILURE_COUNT) {

            if (!deadcom->transmitBytes(frame, frame_len)) {
                deadcom->state = DC_CONNECTED;
                deadcom->send_number = (deadcom->send_number + 7) % 8;
                deadcom->t->mutexUnlock(deadcom->mutex_p);
                return DC_FAILURE;
            }
            bool timed_out;
            if (!deadcom->t->condvarWait(deadcom->condvar_p, DEADCOM_ACK_TIMEOUT_MS, &timed_out)) {
                deadcom->state = DC_CONNECTED;
                deadcom->send_number = (deadcom->send_number + 7) % 8;
                deadcom->t->mutexUnlock(deadcom->mutex_p);
                return DC_FAILURE;
            }

            if (!timed_out) {
                if (deadcom->last_response == DC_RESP_OK) {
                    transmit_success = true;
                    break;
                } else if (deadcom->last_response == DC_RESP_NOLINK) {
                    // The other station dropped link. Let's drop ours
                    transmit_success = false;
                    break;
                }
            }
            deadcom->failure_count++;
        }

        if (transmit_success) {
            // last_acked number was updated by receive thread when handling DC_ACK response
            deadcom->state = DC_CONNECTED;
            if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {return DC_FAILURE;}
            return DC_OK;
        } else {
            // the other station is unresponsive, reset the link.
            resetLink(deadcom);
            if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {return DC_FAILURE;}
            return DC_LINK_RESET;
        }

    } else {
        // Invalid parameters passwd to frame_data function
        deadcom->t->mutexUnlock(deadcom->mutex_p);
        return DC_FAILURE;
    }
}


DeadcomL2Result dcGetReceivedMsg(DeadcomL2 *deadcom, uint8_t *buffer, size_t *msg_len) {
    if (deadcom == NULL || msg_len == NULL) {
        return DC_FAILURE;
    }

    if (!deadcom->t->mutexLock(deadcom->mutex_p)) {return DC_FAILURE;}

    if (!deadcom->extractionComplete) {
        *msg_len = 0;
        if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {return DC_FAILURE;}
        return DC_OK;
    }

    *msg_len = deadcom->extractionBufferSize;

    if (buffer != NULL) {
        memcpy(buffer, deadcom->extractionBuffer, deadcom->extractionBufferSize);

        // acknowledge reception and frame processing
        yahdlc_control_t control_ack = {
            .frame = YAHDLC_FRAME_ACK,
            .recv_seq_no = (deadcom->recv_number + 7) % 8
        };

        size_t ack_frame_length;
        yahdlc_frame_data(&control_ack, NULL, 0, NULL, &ack_frame_length);

        uint8_t ack_frame[ack_frame_length];
        yahdlc_frame_data(&control_ack, NULL, 0, ack_frame, &ack_frame_length);
        if (!deadcom->transmitBytes(ack_frame, ack_frame_length)) {
            deadcom->t->mutexUnlock(deadcom->mutex_p);
            return DC_FAILURE;
        }

        deadcom->extractionBufferSize = 0;
        deadcom->extractionComplete = false;
    }

    if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {
        return DC_FAILURE;
    }

    return DC_OK;
}


DeadcomL2Result dcProcessData(DeadcomL2 *deadcom, const uint8_t *data, size_t len) {
    if (deadcom == NULL || data == NULL || len == 0) {
        return DC_FAILURE;
    }

    if (!deadcom->t->mutexLock(deadcom->mutex_p)) {return DC_FAILURE;}

    size_t processed = 0;
    while (processed < len) {
        yahdlc_control_t frame_control;
        size_t dest_len = 0;
        int yahdlc_result = yahdlc_get_data(&(deadcom->yahdlc_state), &frame_control,
                                            data+processed, len-processed,
                                            deadcom->scratchpadBuffer, &dest_len);

        if (yahdlc_result == -EINVAL) {
            deadcom->t->mutexUnlock(deadcom->mutex_p);
            return DC_FAILURE;
        } else if (yahdlc_result == -EIO) {
            // Invalid frame checksum we should discard `processed_bytes` from the buffer
            processed += dest_len;
        } else if (yahdlc_result == -ENOMSG) {
            // This buffer did not contain end-of-frame mark. It was parsed and we may
            // discard it.
            processed = len;
            break;
        } else {
            // This buffer did contain end of at least one frame. We should discard `yahdlc_result`
            // bytes from the buffer
            processed += yahdlc_result;
            yahdlc_control_t resp_ctrl;
            switch (frame_control.frame) {
                case YAHDLC_FRAME_DATA:
                    // We should process DATA frames only if we are connected
                    if (deadcom->state == DC_CONNECTED || deadcom->state == DC_TRANSMITTING) {
                        if (frame_control.send_seq_no == deadcom->recv_number) {
                            if (deadcom->extractionBufferSize == 0 && dest_len != 0) {
                                deadcom->extractionBufferSize = dest_len;
                                deadcom->extractionComplete = true;
                                memcpy(deadcom->extractionBuffer, deadcom->scratchpadBuffer,
                                       dest_len);
                                deadcom->recv_number = (deadcom->recv_number + 1) % 8;
                            }
                            // else: there already is a message in the extraction buffer. That means
                            // that the other station has sent a frame without waiting for ack on
                            // the previous frame. We will therefore ignore it.
                        } else if (!deadcom->extractionComplete &&
                                   (frame_control.send_seq_no + 1) % 8 == deadcom->recv_number) {
                            // We've seen and previously acked this frame. Since we've received
                            // again that ack must've gotten lost, so retransmit it.
                            yahdlc_control_t control_ack = {
                                .frame = YAHDLC_FRAME_ACK,
                                .recv_seq_no = (deadcom->recv_number + 7) % 8
                            };

                            size_t ack_frame_length;
                            yahdlc_frame_data(&control_ack, NULL, 0, NULL, &ack_frame_length);

                            uint8_t ack_frame[ack_frame_length];
                            yahdlc_frame_data(&control_ack, NULL, 0, ack_frame, &ack_frame_length);
                            if (!deadcom->transmitBytes(ack_frame, ack_frame_length)) {
                                deadcom->t->mutexUnlock(deadcom->mutex_p);
                                return DC_FAILURE;
                            }
                        }
                        // else: It is an out-of-sequence frame, it is safe to ignore.
                    }
                    break;
                case YAHDLC_FRAME_ACK:
                    // We should process ACK frames only if we are transmitting (and therefore
                    // expecting them)
                    if (deadcom->state == DC_TRANSMITTING) {
                        if (frame_control.recv_seq_no == deadcom->next_expected_ack) {
                            // Correct acknowledgment.
                            deadcom->next_expected_ack = (deadcom->next_expected_ack + 1) % 8;
                            deadcom->last_response = DC_RESP_OK;
                            if (!deadcom->t->condvarSignal(deadcom->condvar_p)) {
                                deadcom->t->mutexUnlock(deadcom->mutex_p);
                                return DC_FAILURE;
                            }
                        }
                        // Discard incorrect acknowledgment.
                    }
                    break;
                case YAHDLC_FRAME_NACK:
                    // We should process NACK frames only if we are transmitting (and therefore
                    // expecting them)
                    if (deadcom->state == DC_TRANSMITTING) {
                        // The other station has received some garbage and is proactively requesting
                        // retransmission
                        deadcom->last_response = DC_RESP_REJECT;
                        if (!deadcom->t->condvarSignal(deadcom->condvar_p)) {
                            deadcom->t->mutexUnlock(deadcom->mutex_p);
                            return DC_FAILURE;
                        }
                    }
                    break;
                case YAHDLC_FRAME_CONN:
                    // This is a connection attempt. We should transition to CONNECTED mode
                    // and transmit CONN_ACK frame (even if we already were in the CONNETED mode,
                    // the other station has obviously thought otherwise).
                    resp_ctrl.frame = YAHDLC_FRAME_CONN_ACK;

                    size_t f_len = 0;
                    if (yahdlc_frame_data(&resp_ctrl, NULL, 0, NULL, &f_len) == -EINVAL) {
                        deadcom->t->mutexUnlock(deadcom->mutex_p);
                        return DC_FAILURE;
                    }

                    {
                        uint8_t resp_f[f_len];
                        yahdlc_frame_data(&resp_ctrl, NULL, 0, resp_f, &f_len);
                        if (!deadcom->transmitBytes(resp_f, f_len)) {
                            deadcom->t->mutexUnlock(deadcom->mutex_p);
                            return DC_FAILURE;
                        }
                    }

                    DeadcomL2State original_state = deadcom->state;

                    resetLink(deadcom);
                    deadcom->state = DC_CONNECTED;

                    if (original_state == DC_TRANSMITTING) {
                        // We were transmitting when the link reset happened, we need to notify the
                        // transmit thread.
                        deadcom->last_response = DC_RESP_NOLINK;
                        if (!deadcom->t->condvarSignal(deadcom->condvar_p)) {
                            deadcom->t->mutexUnlock(deadcom->mutex_p);
                            return DC_FAILURE;
                        }
                    } else if (original_state == DC_CONNECTING) {
                        // We were just connecting, so this was a connection comptention. We should
                        // also notify the transmit thread
                        deadcom->last_response = DC_RESP_OK;
                        // The connect thread will handle the state transition
                        deadcom->state = DC_CONNECTING;
                        if (!deadcom->t->condvarSignal(deadcom->condvar_p)) {
                            deadcom->t->mutexUnlock(deadcom->mutex_p);
                            return DC_FAILURE;
                        }
                    }
                    break;
                case YAHDLC_FRAME_CONN_ACK:
                    // If we are connecting the other station has just accepted our connection.
                    // If we weren't connecting then we can safely ignore this.
                    if (deadcom->state == DC_CONNECTING) {
                        deadcom->last_response = DC_RESP_OK;
                        if (!deadcom->t->condvarSignal(deadcom->condvar_p)) {
                            deadcom->t->mutexUnlock(deadcom->mutex_p);
                            return DC_FAILURE;
                        }
                    }
                    break;
            }
        }
    }

    if (!deadcom->t->mutexUnlock(deadcom->mutex_p)) {return DC_FAILURE;}
    return DC_OK;
}
