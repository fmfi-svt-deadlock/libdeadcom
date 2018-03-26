#include <string.h>
#include "unity.h"
#include "fff.h"

#include "dcl2.h"

/*
 * Tests of Deadcom Layer 2 library, part 2: Receiving and processing frames
 */

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(transmitBytes, uint8_t*, uint8_t);
FAKE_VOID_FUNC(mutexInit, void*);
FAKE_VOID_FUNC(mutexLock, void*);
FAKE_VOID_FUNC(mutexUnlock, void*);
FAKE_VOID_FUNC(condvarInit,  void*);
FAKE_VALUE_FUNC(bool, condvarWait, void*, uint32_t);
FAKE_VOID_FUNC(condvarSignal, void*);

FAKE_VALUE_FUNC(int, yahdlc_frame_data, yahdlc_control_t*, const char*, unsigned int, char*,
                unsigned int*);
FAKE_VALUE_FUNC(int, yahdlc_get_data, yahdlc_state_t*, yahdlc_control_t*, const char*, unsigned int,
                char*, unsigned int*);


/* A fake framing implementation that produces inspectable frames in the following format:
 *
 * +-----------------------------+-------------+--------------------------------------------+
 * | control_structure           | message_len | message_bytes                              |
 * | (sizeof(yahdlc_control_t))  | 4 bytes     | message_len                                |
 * +-----------------------------+-------------+--------------------------------------------+
 */
int frame_data_fake_impl(yahdlc_control_t *control, const char *data, unsigned int data_len,
                         char *output_frame, unsigned int *output_frame_len) {

    if (output_frame) {
        // Control struct
        memcpy(output_frame, control, sizeof(yahdlc_control_t));
    }
    *output_frame_len = sizeof(yahdlc_control_t);

    if (data_len != 0) {
        if (output_frame) {
            // Message length
            output_frame[(*output_frame_len)++] = (data_len >> 24) & 0xFF;
            output_frame[(*output_frame_len)++] = (data_len >> 16) & 0xFF;
            output_frame[(*output_frame_len)++] = (data_len >>  8) & 0xFF;
            output_frame[(*output_frame_len)++] = (data_len >>  0) & 0xFF;
        } else {
            *output_frame_len += 4;
        }

        if (output_frame) {
            // The message itself
            memcpy(output_frame+(*output_frame_len), data, data_len);
        }
        *output_frame_len += data_len;
    }
}

DeadcomL2ThreadingVMT t = {
    &mutexInit,
    &mutexLock,
    &mutexUnlock,
    &condvarInit,
    &condvarWait,
    &condvarSignal
};


int get_data_fake_data_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                  unsigned int src_len, char* dest, unsigned int *dest_len) {
    uint8_t data[] = {0, 1, 2, 3, 4, 5};
    control->frame = YAHDLC_FRAME_DATA;
    control->recv_seq_no = 0;
    control->send_seq_no = 0;
    memcpy(dest, data, sizeof(data));
    *dest_len = sizeof(data);
    return src_len;
}

int get_data_fake_ack_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                  unsigned int src_len, char* dest, unsigned int *dest_len) {
    control->frame = YAHDLC_FRAME_ACK;
    control->recv_seq_no = 0;
    *dest_len = 0;
    return src_len;
}

int get_data_fake_nack_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                  unsigned int src_len, char* dest, unsigned int *dest_len) {
    control->frame = YAHDLC_FRAME_NACK;
    *dest_len = 0;
    return src_len;
}

int get_data_fake_conn_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                  unsigned int src_len, char* dest, unsigned int *dest_len) {
    control->frame = YAHDLC_FRAME_CONN;
    *dest_len = 0;
    return src_len;
}

int get_data_fake_connack_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                  unsigned int src_len, char* dest, unsigned int *dest_len) {
    control->frame = YAHDLC_FRAME_CONN_ACK;
    *dest_len = 0;
    return src_len;
}

int get_data_fake_disconnected_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                  unsigned int src_len, char* dest, unsigned int *dest_len) {
    control->frame = YAHDLC_FRAME_DISCONNECTED;
    *dest_len = 0;
    return src_len;
}


/* == Generic processData tests ==================================================================*/

// TODO what if yahdlc_get_data returns 0?

void test_PDInvalidParams() {
    DeadcomL2 d;
    uint8_t data[47] = {0};

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    TEST_ASSERT_EQUAL(DC_FAILURE, dcProcessData(NULL, data, 47));
    TEST_ASSERT_EQUAL(DC_FAILURE, dcProcessData(&d, NULL, 47));
    TEST_ASSERT_EQUAL(DC_FAILURE, dcProcessData(&d, data, 0));
}


void test_PDFailsOnYahdlcFailure() {
    DeadcomL2 d;
    uint8_t data[] = {0, 1, 2, 3, 4, 5};

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);

    unsigned int seen_bytes = 0;
    int get_data_fake(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                      unsigned int src_len, char* dest, unsigned int *dest_len) {
        return -EINVAL;
    }
    yahdlc_get_data_fake.custom_fake = &get_data_fake;

    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    TEST_ASSERT_EQUAL(DC_FAILURE, dcProcessData(&d, data, sizeof(data)));
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
}


void test_PDProcessesWholeBuffer() {
    DeadcomL2 d;
    uint8_t data[] = {0, 1, 2, 3, 4, 5};

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);

    unsigned int seen_bytes = 0;
    int get_data_fake(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                      unsigned int src_len, char* dest, unsigned int *dest_len) {
        TEST_ASSERT_EQUAL(seen_bytes, src[0]);
        TEST_ASSERT_EQUAL(seen_bytes+1, src[1]);
        seen_bytes += 2;
        *dest_len = 2;
        return -ENOMSG;
    }
    yahdlc_get_data_fake.custom_fake = &get_data_fake;

    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, data, sizeof(data)));
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
}


/* == Frame processing in Disconnected mode ======================================================*/

void test_PDProcessDataWhenDisconnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_data_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
}


void test_PDProcessAckWhenDisconnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_ack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
}


void test_PDProcessNackWhenDisconnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_nack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
}


void test_PDProcessConnWhenDisconnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_conn_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    void transmitBytes_fake_impl(uint8_t *data, uint8_t len) {
        // We should have transmitted CONN_ACK frame as response
        TEST_ASSERT_EQUAL(YAHDLC_FRAME_CONN_ACK, ((yahdlc_control_t*)data)->frame);
    }
    transmitBytes_fake.custom_fake = &transmitBytes_fake_impl;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(1, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
}


void test_PDProcessConnAckWhenDisconnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_connack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
}


void test_PDProcessDisconnectedWhenDisconnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_disconnected_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // We should have ignored this frame
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
}


/* == Frame processing in Connecting mode ========================================================*/

void test_PDProcessDataWhenConnecting() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(condvarSignal);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_data_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTING;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // This frame should have been ignored
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    // Condvar should not have been signaled
    TEST_ASSERT_EQUAL(0, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTING, d.state);
}


void test_PDProcessAckWhenConnecting() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_ack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTING;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // This frame should have been ignored
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    // Condvar should not have been signaled
    TEST_ASSERT_EQUAL(0, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTING, d.state);
}


void test_PDProcessNackWhenConnecting() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_nack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTING;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // This frame should have been ignored
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    // Condvar should not have been signaled
    TEST_ASSERT_EQUAL(0, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTING, d.state);
}


void test_PDProcessConnWhenConnecting() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_conn_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTING;

    void transmitBytes_fake_impl(uint8_t *data, uint8_t len) {
        // We should have transmitted CONN_ACK frame as response
        TEST_ASSERT_EQUAL(YAHDLC_FRAME_CONN_ACK, ((yahdlc_control_t*)data)->frame);
    }
    transmitBytes_fake.custom_fake = &transmitBytes_fake_impl;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(1, transmitBytes_fake.call_count);
    // And signal the condvar because this also means that the connection succeeded
    TEST_ASSERT_EQUAL(1, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTING, d.state);
    TEST_ASSERT_EQUAL(DC_RESP_OK, d.last_response);
}


void test_PDProcessConnAckWhenConnecting() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_connack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTING;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTING, d.state);
    TEST_ASSERT_EQUAL(1, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(DC_RESP_OK, d.last_response);
}


/* == Frame processing in Connected mode (except Data frames) =====================================*/

void test_PDProcessAckWhenConnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_ack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTED;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // This frame should have been ignored
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    // Condvar should not have been signaled
    TEST_ASSERT_EQUAL(0, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
}


void test_PDProcessNackWhenConnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_nack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTED;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // This frame should have been ignored
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    // Condvar should not have been signaled
    TEST_ASSERT_EQUAL(0, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
}


void test_PDProcessConnWhenConnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_conn_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTED;
    d.send_number = 3;

    void transmitBytes_fake_impl(uint8_t *data, uint8_t len) {
        // We should have transmitted CONN_ACK frame as response
        TEST_ASSERT_EQUAL(YAHDLC_FRAME_CONN_ACK, ((yahdlc_control_t*)data)->frame);
    }
    transmitBytes_fake.custom_fake = &transmitBytes_fake_impl;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(1, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(0, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
    // Check that link state was reset
    TEST_ASSERT_EQUAL(0, d.send_number);
}


void test_PDProcessConnAckWhenConnected() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_connack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTED;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
}


/* == Frame processing in Transmitting mode (execpt Data frames) =================================*/

void test_PDProcessAckWhenTransmitting() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_ack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_TRANSMITTING;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(DC_RESP_OK, d.last_response);
    TEST_ASSERT_EQUAL(1, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_TRANSMITTING, d.state);
}


void test_PDProcessNackWhenTransmitting() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_nack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_TRANSMITTING;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(DC_RESP_REJECT, d.last_response);
    TEST_ASSERT_EQUAL(1, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_TRANSMITTING, d.state);
}


void test_PDProcessConnWhenTransmitting() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_conn_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_TRANSMITTING;
    d.send_number = 3;

    void transmitBytes_fake_impl(uint8_t *data, uint8_t len) {
        // We should have transmitted CONN_ACK frame as response
        TEST_ASSERT_EQUAL(YAHDLC_FRAME_CONN_ACK, ((yahdlc_control_t*)data)->frame);
    }
    transmitBytes_fake.custom_fake = &transmitBytes_fake_impl;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(1, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(DC_RESP_NOLINK, d.last_response);
    TEST_ASSERT_EQUAL(1, condvarSignal_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
    // Check that link state was reset
    TEST_ASSERT_EQUAL(0, d.send_number);
}


void test_PDProcessConnAckWhenTransmitting(){
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);
    yahdlc_get_data_fake.custom_fake = &get_data_fake_connack_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_TRANSMITTING;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DC_TRANSMITTING, d.state);
}


/* == Data frame processing in Connected and Transmitting modes ==================================*/


void test_PDDataCorrectSeq() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);

    int get_data_fake_data_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                      unsigned int src_len, char* dest, unsigned int *dest_len) {
        uint8_t data[] = {0, 1, 2, 3, 4, 5};
        control->frame = YAHDLC_FRAME_DATA;
        control->send_seq_no = 0;
        control->recv_seq_no = 0;
        memcpy(dest, data, sizeof(data));
        *dest_len = sizeof(data);
        return src_len;
    }

    yahdlc_get_data_fake.custom_fake = &get_data_fake_data_frame;

    d.state = DC_CONNECTED;
    d.recv_number = 0;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // No response till we read that frame from the buffer
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    // A message should be waiting for us
    TEST_ASSERT_EQUAL(6, dcGetReceivedMsgLen(&d));
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
}


void test_PDDataAlreadySeenNotAcked() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);

    int get_data_fake_data_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                      unsigned int src_len, char* dest, unsigned int *dest_len) {
        uint8_t data[] = {0, 1, 2, 3, 4, 5};
        control->frame = YAHDLC_FRAME_DATA;
        control->send_seq_no = 0;
        control->recv_seq_no = 0;
        memcpy(dest, data, sizeof(data));
        *dest_len = sizeof(data);
        return src_len;
    }

    yahdlc_get_data_fake.custom_fake = &get_data_fake_data_frame;

    d.state = DC_CONNECTED;
    d.recv_number = 1;
    d.extractionComplete = true;
    d.extractionBufferSize = 6;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // We've received a retransmission of a frame we've already seen and not yet acked
    // We should've ignored this frame
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    // A message should still be waiting for us
    TEST_ASSERT_EQUAL(6, dcGetReceivedMsgLen(&d));
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
}


void test_PDDataAlreadySeenAndAcked() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);

    int get_data_fake_data_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                      unsigned int src_len, char* dest, unsigned int *dest_len) {
        uint8_t data[] = {0, 1, 2, 3, 4, 5};
        control->frame = YAHDLC_FRAME_DATA;
        control->send_seq_no = 0;
        control->recv_seq_no = 0;
        memcpy(dest, data, sizeof(data));
        *dest_len = sizeof(data);
        return src_len;
    }

    void transmitBytes_fake_impl(uint8_t *data, uint8_t len) {
        // We should have transmitted CONN_ACK frame as response
        TEST_ASSERT_EQUAL(YAHDLC_FRAME_ACK, ((yahdlc_control_t*)data)->frame);
        TEST_ASSERT_EQUAL(0, ((yahdlc_control_t*)data)->recv_seq_no);
    }
    transmitBytes_fake.custom_fake = &transmitBytes_fake_impl;
    yahdlc_get_data_fake.custom_fake = &get_data_fake_data_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTED;
    d.recv_number = 1;
    d.extractionComplete = false;
    d.extractionBufferSize = DC_E_NOMSG;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // We've received a retransmission of a frame we've already acked. That ack must've gotten lost.
    // We should retransmit the ack.
    TEST_ASSERT_EQUAL(1, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    // No message should've appeared in the buffer
    TEST_ASSERT_EQUAL(DC_E_NOMSG, dcGetReceivedMsgLen(&d));
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
}


void test_PDDataReceiveWithoutAck() {
    uint8_t dummy[] = {0};
    uint8_t data1[] = {0, 1, 2, 3, 4, 5};
    uint8_t data2[] = {5, 6, 7};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);

    int get_data_fake_data_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                      unsigned int src_len, char* dest, unsigned int *dest_len) {
        control->frame = YAHDLC_FRAME_DATA;
        control->send_seq_no = 0;
        control->recv_seq_no = 0;
        memcpy(dest, data1, sizeof(data1));
        *dest_len = sizeof(data1);
        return src_len;
    }
    yahdlc_get_data_fake.custom_fake = &get_data_fake_data_frame;

    d.state = DC_CONNECTED;
    d.recv_number = 0;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // No response till we read that frame from the buffer
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    // A message should be waiting for us
    TEST_ASSERT_EQUAL(6, dcGetReceivedMsgLen(&d));
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
    TEST_ASSERT_EQUAL(1, d.recv_number);


    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);

    int get_data_fake_data_frame2(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                      unsigned int src_len, char* dest, unsigned int *dest_len) {
        control->frame = YAHDLC_FRAME_DATA;
        control->send_seq_no = 1;
        control->recv_seq_no = 0;
        memcpy(dest, data2, sizeof(data2));
        *dest_len = sizeof(data2);
        return src_len;
    }
    yahdlc_get_data_fake.custom_fake = &get_data_fake_data_frame2;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // We are ignoring this frame
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    // The original message should still be waiting for us
    TEST_ASSERT_EQUAL(6, dcGetReceivedMsgLen(&d));
    TEST_ASSERT_EQUAL_MEMORY(data1, d.extractionBuffer, sizeof(data1));
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
    TEST_ASSERT_EQUAL(1, d.recv_number);
}


void test_PDDataIncorrectSeq() {
    uint8_t dummy[] = {0};
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(yahdlc_get_data);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(condvarSignal);

    int get_data_fake_data_frame(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                      unsigned int src_len, char* dest, unsigned int *dest_len) {
        uint8_t data[] = {0, 1, 2, 3, 4, 5};
        control->frame = YAHDLC_FRAME_DATA;
        control->send_seq_no = 5;
        control->recv_seq_no = 5;
        memcpy(dest, data, sizeof(data));
        *dest_len = sizeof(data);
        return src_len;
    }

    yahdlc_get_data_fake.custom_fake = &get_data_fake_data_frame;
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    d.state = DC_CONNECTED;
    d.recv_number = 1;
    d.extractionComplete = false;
    d.extractionBufferSize = DC_E_NOMSG;

    TEST_ASSERT_EQUAL(DC_OK, dcProcessData(&d, dummy, 1));
    // We've received an out-of-sequence frame, ignore it.
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    // No message should've appeared in the buffer
    TEST_ASSERT_EQUAL(DC_E_NOMSG, dcGetReceivedMsgLen(&d));
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
}
