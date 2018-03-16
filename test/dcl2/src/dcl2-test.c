#include <string.h>
#include "unity.h"
#include "fff.h"

#include "dcl2.h"

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(transmitBytes, uint8_t*, uint8_t);
FAKE_VOID_FUNC(mutexInit, void*);
FAKE_VOID_FUNC(mutexLock, void*);
FAKE_VOID_FUNC(mutexUnlock, void*);
FAKE_VOID_FUNC(condvarInit,  void*);
FAKE_VALUE_FUNC(bool, condvarWait, void*, uint32_t);
FAKE_VOID_FUNC(condvarSignal, void*);

FAKE_VALUE_FUNC(int, yahdlc_frame_data, yahdlc_control_t*, const char*, unsigned int, char*,
                unsigned int*)

/* A fake framing implementation that produces inspectable frames in the following format:
 *
 * +-----------------------------+-------------+--------------------------------------------+
 * | control_structure           | message_len | message_bytes                              |
 * | (sizeof(yahdlc_control_t))  | 4 bytes     | message_len                                |
 * +-----------------------------+-------------+--------------------------------------------+
 */
int frame_data_fake_impl(yahdlc_control_t *control, const char *data, unsigned int data_len,
                         char *output_frame, unsigned int *output_frame_len) {
    // Control struct
    memcpy(output_frame, control, sizeof(yahdlc_control_t));
    *output_frame_len = sizeof(yahdlc_control_t);

    if (data_len != 0) {
        // Message length
        output_frame[(*output_frame_len)++] = (data_len >> 24) & 0xFF;
        output_frame[(*output_frame_len)++] = (data_len >> 16) & 0xFF;
        output_frame[(*output_frame_len)++] = (data_len >>  8) & 0xFF;
        output_frame[(*output_frame_len)++] = (data_len >>  0) & 0xFF;

        // The message itself
        memcpy(output_frame+(*output_frame_len), data, data_len);
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

// TODO all asserts are backwards, expected and actial are swapped...

/* == Library initialization =====================================================================*/

void test_ValidInit() {
    DeadcomL2 d;

    RESET_FAKE(mutexInit);
    RESET_FAKE(condvarInit);
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);

    TEST_ASSERT_EQUAL(res, DC_OK);
    TEST_ASSERT_EQUAL(d.state, DC_DISCONNECTED);
    TEST_ASSERT_EQUAL(d.transmitBytes, &transmitBytes);
    TEST_ASSERT_EQUAL(d.t, &t);
    TEST_ASSERT_EQUAL(d.mutex_p, 1);
    TEST_ASSERT_EQUAL(d.condvar_p, 2);
    TEST_ASSERT_EQUAL(mutexInit_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexInit_fake.arg0_val, 1);
    TEST_ASSERT_EQUAL(condvarInit_fake.call_count, 1);
    TEST_ASSERT_EQUAL(condvarInit_fake.arg0_val, 2);
}


void test_InvalidInit() {
    DeadcomL2Result res = dcInit(NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(res, DC_FAILURE);
}


/* == Connection establishment ===================================================================*/

void test_InvalidConnectionParams() {
    DeadcomL2Result res = dcConnect(NULL);
    TEST_ASSERT_EQUAL(res, DC_FAILURE);
}


void test_ValidConnection() {
    DeadcomL2 d;

    RESET_FAKE(mutexInit);
    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(condvarInit);
    RESET_FAKE(condvarWait);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);

    int frame_data_fake_impl(yahdlc_control_t *control, const char *data, unsigned int data_len,
                        char *data_out, unsigned int *data_out_len) {
        // this function should request creation of CONN frames only
        TEST_ASSERT_EQUAL(control->frame, YAHDLC_FRAME_CONN);
        // with no data
        TEST_ASSERT_EQUAL(data_len, 0);
        // Lets return fake frame
        data_out[0] = 0x42;
        data_out[1] = 0x47;
        *data_out_len = 2;
    }
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    void transmitBytes_fake_impl(uint8_t *data, uint8_t len) {
        // this function should onle ever attempt to transmit fake frame generated above
        TEST_ASSERT_EQUAL(len, 2);
        uint8_t fake_frame[] = {0x42, 0x47};
        TEST_ASSERT_EQUAL_MEMORY(fake_frame, data, 2);
    }
    transmitBytes_fake.custom_fake = &transmitBytes_fake_impl;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // The condvarWait should return `true`, thereby simulating reception of connection ack
    SET_RETURN_SEQ(condvarWait, (bool[1]){true}, 1);

    // Simulate connection attempt
    res = dcConnect(&d);

    // Under these circumstances the connection attemt should have succeeded
    TEST_ASSERT_EQUAL(res, DC_OK);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexLock_fake.arg0_val, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.arg0_val, 1);
    // The function should have waited on condition variable exactly once
    TEST_ASSERT_EQUAL(condvarWait_fake.call_count, 1);
    TEST_ASSERT_EQUAL(condvarWait_fake.arg0_val, 2);
    // Status should be connected
    TEST_ASSERT_EQUAL(d.state, DC_CONNECTED);
}


void test_ConnectionNoResponse() {
    // Frame construction and transmission is already verified in `test_ValidConnection`. Here we
    // verify only what happens when we won't receive a response.
    DeadcomL2 d;

    RESET_FAKE(mutexInit);
    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(condvarInit);
    RESET_FAKE(condvarWait);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // The condvarWait should return `false`, thereby simulating no connection request ack
    SET_RETURN_SEQ(condvarWait, (bool[1]){false}, 1);
    // Simulate connection attempt
    res = dcConnect(&d);

    // Under these circumstances the connection attemt should have failed
    TEST_ASSERT_EQUAL(res, DC_NOT_CONNECTED);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexLock_fake.arg0_val, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.arg0_val, 1);
    // The function should have waited on condition variable exactly once
    TEST_ASSERT_EQUAL(condvarWait_fake.call_count, 1);
    TEST_ASSERT_EQUAL(condvarWait_fake.arg0_val, 2);
    // Status should be connected
    TEST_ASSERT_EQUAL(d.state, DC_DISCONNECTED);
}


void test_ConnectionNoOpWhenConnected() {
    DeadcomL2 d;

    RESET_FAKE(mutexInit);
    RESET_FAKE(condvarWait);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // The condvarWait should return `true`, thereby simulating reception of connection ack
    SET_RETURN_SEQ(condvarWait, (bool[1]){true}, 1);
    // Simulate first connection attempt
    res = dcConnect(&d);
    // which should be successfull
    TEST_ASSERT_EQUAL(res, DC_OK);
    TEST_ASSERT_EQUAL(d.state, DC_CONNECTED);

    // and now try calling the connect function again
    res = dcConnect(&d);
    // return should be success and state connected
    TEST_ASSERT_EQUAL(res, DC_OK);
    TEST_ASSERT_EQUAL(d.state, DC_CONNECTED);
    // however functions should have been called only once
    TEST_ASSERT_EQUAL(condvarWait_fake.call_count, 1);
    TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 1);
    TEST_ASSERT_EQUAL(yahdlc_frame_data_fake.call_count, 1);
}


void test_ConnectionFailWhenAlreadyConnecting() {
    DeadcomL2 d;

    RESET_FAKE(mutexInit);
    RESET_FAKE(condvarWait);
    RESET_FAKE(yahdlc_frame_data);
    RESET_FAKE(transmitBytes);

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // Simulate DC_CONNECTING state
    d.state = DC_CONNECTING;

    // The condvarWait should return `true`, thereby simulating reception of connection ack
    // if ever called
    SET_RETURN_SEQ(condvarWait, (bool[1]){true}, 1);
    res = dcConnect(&d);
    // return should be failure
    TEST_ASSERT_EQUAL(res, DC_FAILURE);
    // and no frame / transmit / wait functions should have been called
    TEST_ASSERT_EQUAL(condvarWait_fake.call_count, 0);
    TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 0);
    TEST_ASSERT_EQUAL(yahdlc_frame_data_fake.call_count, 0);
}


/* == Dropping connection ========================================================================*/

void test_disconnectWhenDisconnected() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // Attempt to disconnect already disconnected link
    res = dcDisconnect(&d);

    // result should be OK
    TEST_ASSERT_EQUAL(res, DC_OK);
    // State should be disconnected
    TEST_ASSERT_EQUAL(d.state, DC_DISCONNECTED);
    // Mutex should be locked / unlocked exactly once
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
}


void test_disconnectWhenConnecting() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // Simulate connecting link
    d.state = DC_CONNECTING;

    // Attempt to disconnect connecting link
    res = dcDisconnect(&d);

    // result should be failure
    TEST_ASSERT_EQUAL(res, DC_FAILURE);
    // state should be unchanged
    TEST_ASSERT_EQUAL(d.state, DC_CONNECTING);
    // mutex should be locked / unlocked exactly once
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
}


void test_disconnectWhenConnected() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // Simulate connected link
    d.state = DC_CONNECTED;

    // Attempt to disconnect already disconnected link
    res = dcDisconnect(&d);

    // result should be OK
    TEST_ASSERT_EQUAL(res, DC_OK);
    // State should be disconnected
    TEST_ASSERT_EQUAL(d.state, DC_DISCONNECTED);
    // Mutex should be locked / unlocked exactly once
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
}



/* == Sending a message ==========================================================================*/

void test_SendMessageInvalidParams() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(transmitBytes);

    const uint8_t message[] = {0x42, 0x47};

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    res = dcSendMessage(NULL, message, sizeof(message));
    // failed call, no locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(res, DC_FAILURE);
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 0);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
    TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 0);

    res = dcSendMessage(&d, NULL, sizeof(message));
    // failed call, no locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(res, DC_FAILURE);
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 0);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
    TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 0);

    res = dcSendMessage(&d, message, 0);
    // failed call, no locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(res, DC_FAILURE);
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 0);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
    TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 0);

    res = dcSendMessage(NULL, message, DEADCOM_PAYLOAD_MAX_LEN+1);
    // failed call, no locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(res, DC_FAILURE);
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 0);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
    TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 0);
}


void test_SendMessageInvalidState() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(transmitBytes);

    const uint8_t message[] = {0x42, 0x47};

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // Disconnected state
    res = dcSendMessage(&d, message, sizeof(message));
    // failed call, one of each locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(res, DC_NOT_CONNECTED);
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 0);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    d.state = DC_CONNECTING;
    // Connecting state
    res = dcSendMessage(&d, message, sizeof(message));
    // failed call, one of each locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(res, DC_NOT_CONNECTED);
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 0);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    d.state = DC_TRANSMITTING;
    // Connected but transmitting
    res = dcSendMessage(&d, message, sizeof(message));
    // failed call, one of each locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(res, DC_FAILURE);
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 0);
}


void test_SendMessageWithAcknowledgment() {
    DeadcomL2 d;

    unsigned int sseq, rseq;
    for (sseq = 0; sseq <= 7; sseq++) {
        for (rseq = 0; rseq <= 7; rseq++) {
            RESET_FAKE(mutexLock);
            RESET_FAKE(mutexUnlock);
            RESET_FAKE(condvarWait);
            RESET_FAKE(transmitBytes);
            RESET_FAKE(yahdlc_frame_data)
            yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

            const uint8_t message[] = {0x42, 0x47};

            bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
                // Fake implementation of condvarWait function.
                // This function should be called after the message is transmitted.
                // The mutex should be locked
                TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
                TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
                // Bytes should have been transmitted
                TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 1);
                // Library status should be 'transmitting'
                TEST_ASSERT_EQUAL(d.state, DC_TRANSMITTING);

                // Verify that proper message has been transmitted
                uint8_t *txed_bytes = transmitBytes_fake.arg0_val;
                unsigned int num_txed_bytes = transmitBytes_fake.arg1_val;

                // Verify that this is a DATA frame with correct sequence numbers
                TEST_ASSERT_EQUAL(((yahdlc_control_t*)txed_bytes)->frame, YAHDLC_FRAME_DATA);
                TEST_ASSERT_EQUAL(((yahdlc_control_t*)txed_bytes)->send_seq_no, sseq);
                TEST_ASSERT_EQUAL(((yahdlc_control_t*)txed_bytes)->recv_seq_no, rseq);

                // Verify that we've sent 2 bytes
                TEST_ASSERT_EQUAL(txed_bytes[sizeof(yahdlc_control_t)+3], 2);
                // Verify that we've sent the correct message
                TEST_ASSERT_EQUAL_MEMORY((txed_bytes + sizeof(yahdlc_control_t) + 4), message, 2);

                // The other station acknowledged
                d.last_response = DC_RESP_OK;
                return true;
            }
            condvarWait_fake.custom_fake = &condvarWait_fakeimpl;

            // Initialize the lib
            DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
            TEST_ASSERT_EQUAL(res, DC_OK);

            // Simulate connected state
            d.state = DC_CONNECTED;
            d.send_number = sseq;
            d.last_acked  = sseq;
            d.recv_number = rseq;

            res = dcSendMessage(&d, message, sizeof(message));
            // Transmission should be successful
            TEST_ASSERT_EQUAL(res, DC_OK);
            // Mutex should have been locked and unlocked exactly once
            TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
            TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
            // We should have waited on condition variable exactly once
            TEST_ASSERT_EQUAL(condvarWait_fake.call_count, 1);
            // Library status should be 'connected'
            TEST_ASSERT_EQUAL(d.state, DC_CONNECTED);
        }
    }
}


void test_SendMessageWhenNacked() {
    DeadcomL2 d;

    unsigned int nack_number;
    for (nack_number = 1; nack_number < DEADCOM_MAX_FAILURE_COUNT; nack_number++) {
        RESET_FAKE(mutexLock);
        RESET_FAKE(mutexUnlock);
        RESET_FAKE(condvarWait);
        RESET_FAKE(transmitBytes);
        RESET_FAKE(yahdlc_frame_data)
        yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

        const uint8_t message[] = {0x42, 0x47};

        unsigned int nacked_times = 0;
        bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
            // Fake implementation of condvarWait function.
            // This function should be called after the message is transmitted.
            // The mutex should be locked
            TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
            TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
            // Bytes should have been transmitted
            TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, (nacked_times+1));
            // Library status should be 'transmitting'
            TEST_ASSERT_EQUAL(d.state, DC_TRANSMITTING);

            // Verify that proper message has been transmitted
            uint8_t *txed_bytes = transmitBytes_fake.arg0_val;
            unsigned int num_txed_bytes = transmitBytes_fake.arg1_val;

            // Verify that this is a DATA frame with correct sequence numbers
            TEST_ASSERT_EQUAL(((yahdlc_control_t*)txed_bytes)->frame, YAHDLC_FRAME_DATA);
            TEST_ASSERT_EQUAL(((yahdlc_control_t*)txed_bytes)->send_seq_no, 0);
            TEST_ASSERT_EQUAL(((yahdlc_control_t*)txed_bytes)->recv_seq_no, 0);

            // Verify that we've sent 2 bytes
            TEST_ASSERT_EQUAL(txed_bytes[sizeof(yahdlc_control_t)+3], 2);
            // Verify that we've sent the correct message
            TEST_ASSERT_EQUAL_MEMORY((txed_bytes + sizeof(yahdlc_control_t) + 4), message, 2);

            if (nacked_times == nack_number) {
                d.last_response = DC_RESP_OK;
            } else {
                d.last_response = DC_RESP_REJECT;
                nacked_times++;
            }
            return true;
        }
        condvarWait_fake.custom_fake = &condvarWait_fakeimpl;

        // Initialize the lib
        DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
        TEST_ASSERT_EQUAL(res, DC_OK);

        // Simulate connected state
        d.state = DC_CONNECTED;

        res = dcSendMessage(&d, message, sizeof(message));
        // Transmission should be successful
        TEST_ASSERT_EQUAL(res, DC_OK);
        // Mutex should have been locked and unlocked exactly once
        TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
        TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
        // We should have waited on condition variable after each nack and once more for good send
        TEST_ASSERT_EQUAL(condvarWait_fake.call_count, (nack_number+1));
        // Library status should be 'connected'
        TEST_ASSERT_EQUAL(d.state, DC_CONNECTED);
    }
}


void test_SendMessageWhenTimeout() {
    DeadcomL2 d;

    unsigned int timeout_number;
    for (timeout_number = 1; timeout_number < DEADCOM_MAX_FAILURE_COUNT; timeout_number++) {
        RESET_FAKE(mutexLock);
        RESET_FAKE(mutexUnlock);
        RESET_FAKE(condvarWait);
        RESET_FAKE(transmitBytes);
        RESET_FAKE(yahdlc_frame_data)
        yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

        const uint8_t message[] = {0x42, 0x47};

        unsigned int timeout_times = 0;
        bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
            // Fake implementation of condvarWait function.
            // This function should be called after the message is transmitted.
            // The mutex should be locked
            TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
            TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
            // Bytes should have been transmitted
            TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, (timeout_times+1));
            // Library status should be 'transmitting'
            TEST_ASSERT_EQUAL(d.state, DC_TRANSMITTING);

            // Verify that proper message has been transmitted
            uint8_t *txed_bytes = transmitBytes_fake.arg0_val;
            unsigned int num_txed_bytes = transmitBytes_fake.arg1_val;

            // Verify that this is a DATA frame with correct sequence numbers
            TEST_ASSERT_EQUAL(((yahdlc_control_t*)txed_bytes)->frame, YAHDLC_FRAME_DATA);
            TEST_ASSERT_EQUAL(((yahdlc_control_t*)txed_bytes)->send_seq_no, 0);
            TEST_ASSERT_EQUAL(((yahdlc_control_t*)txed_bytes)->recv_seq_no, 0);

            // Verify that we've sent 2 bytes
            TEST_ASSERT_EQUAL(txed_bytes[sizeof(yahdlc_control_t)+3], 2);
            // Verify that we've sent the correct message
            TEST_ASSERT_EQUAL_MEMORY((txed_bytes + sizeof(yahdlc_control_t) + 4), message, 2);

            if (timeout_times == timeout_number) {
                d.last_response = DC_RESP_OK;
                return true;
            } else {
                timeout_times++;
            }
            return false;
        }
        condvarWait_fake.custom_fake = &condvarWait_fakeimpl;

        // Initialize the lib
        DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
        TEST_ASSERT_EQUAL(res, DC_OK);

        // Simulate connected state
        d.state = DC_CONNECTED;

        res = dcSendMessage(&d, message, sizeof(message));
        // Transmission should be successful
        TEST_ASSERT_EQUAL(res, DC_OK);
        // Mutex should have been locked and unlocked exactly once
        TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
        TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
        // We should have waited on condition variable after each nack and once more for good send
        TEST_ASSERT_EQUAL(condvarWait_fake.call_count, (timeout_number+1));
        // Library status should be 'connected'
        TEST_ASSERT_EQUAL(d.state, DC_CONNECTED);
    }
}


void test_SendMessageFailTooManyNacks() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(condvarWait);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(yahdlc_frame_data)
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    const uint8_t message[] = {0x42, 0x47};

    unsigned int timeout_times = 0;
    bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
        // Fake implementation of condvarWait function.
        // This function should be called after the message is transmitted.
        // The mutex should be locked
        TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
        TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
        // Bytes should have been transmitted
        TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, (timeout_times+1));
        // Library status should be 'transmitting'
        TEST_ASSERT_EQUAL(d.state, DC_TRANSMITTING);

        if (timeout_times >= DEADCOM_MAX_FAILURE_COUNT) {
            // At this point the send function should have given up and we shouldn't be here
            d.last_response = DC_RESP_OK;
            TEST_FAIL();
        } else {
            d.last_response = DC_RESP_REJECT;
            timeout_times++;
        }
        return true;
    }
    condvarWait_fake.custom_fake = &condvarWait_fakeimpl;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // Simulate connected state
    d.state = DC_CONNECTED;

    res = dcSendMessage(&d, message, sizeof(message));
    // Transmission should fail with link reset error
    TEST_ASSERT_EQUAL(res, DC_LINK_RESET);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(condvarWait_fake.call_count, DEADCOM_MAX_FAILURE_COUNT);
    // Library status should be 'disconnected'
    TEST_ASSERT_EQUAL(d.state, DC_DISCONNECTED);
}


void test_SendMessageFailTooManyTimeouts() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(condvarWait);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(yahdlc_frame_data)
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    const uint8_t message[] = {0x42, 0x47};

    unsigned int timeout_times = 0;
    bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
        // Fake implementation of condvarWait function.
        // This function should be called after the message is transmitted.
        // The mutex should be locked
        TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
        TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
        // Bytes should have been transmitted
        TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, (timeout_times+1));
        // Library status should be 'transmitting'
        TEST_ASSERT_EQUAL(d.state, DC_TRANSMITTING);

        if (timeout_times >= DEADCOM_MAX_FAILURE_COUNT) {
            // At this point the send function should have given up and we shouldn't be here
            d.last_response = DC_RESP_OK;
            TEST_FAIL();
            return true;
        } else {
            timeout_times++;
        }
        return false;
    }
    condvarWait_fake.custom_fake = &condvarWait_fakeimpl;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // Simulate connected state
    d.state = DC_CONNECTED;

    res = dcSendMessage(&d, message, sizeof(message));
    // Transmission should fail with link reset error
    TEST_ASSERT_EQUAL(res, DC_LINK_RESET);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(condvarWait_fake.call_count, DEADCOM_MAX_FAILURE_COUNT);
    // Library status should be 'disconnected'
    TEST_ASSERT_EQUAL(d.state, DC_DISCONNECTED);
}


void test_SendMessageOtherStationDropsLink() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);
    RESET_FAKE(condvarWait);
    RESET_FAKE(transmitBytes);
    RESET_FAKE(yahdlc_frame_data)
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    const uint8_t message[] = {0x42, 0x47};

    bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
        // Fake implementation of condvarWait function.
        // This function should be called after the message is transmitted.
        // The mutex should be locked
        TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
        TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 0);
        // Bytes should have been transmitted
        TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 1);
        // Library status should be 'transmitting'
        TEST_ASSERT_EQUAL(d.state, DC_TRANSMITTING);

        d.last_response = DC_RESP_NOLINK;
        return true;
    }
    condvarWait_fake.custom_fake = &condvarWait_fakeimpl;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // Simulate connected state
    d.state = DC_CONNECTED;

    res = dcSendMessage(&d, message, sizeof(message));
    // Transmission should fail with link reset error
    TEST_ASSERT_EQUAL(res, DC_LINK_RESET);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(condvarWait_fake.call_count, 1);
    // Library status should be 'disconnected'
    TEST_ASSERT_EQUAL(d.state, DC_DISCONNECTED);
}


/* == Getting previously received message ========================================================*/

void test_GetMessageLengthInvalidParams() {
    TEST_ASSERT_EQUAL(dcGetReceivedMsgLen(NULL), DC_E_FAIL);
}


void test_GetMessageLengthNoMessagePending() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    TEST_ASSERT_EQUAL(dcGetReceivedMsgLen(&d), DC_E_NOMSG);

    // Mutex should have been locked and unlocked
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
}


void test_GetMessageLength() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    // Simulate that we've received a message
    d.extractionBufferSize = 47;

    TEST_ASSERT_EQUAL(dcGetReceivedMsgLen(&d), 47);

    // Mutex should have been locked and unlocked
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
}


void test_GetMessageInvalidParams() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    uint8_t buffer[47];

    TEST_ASSERT_EQUAL(dcGetReceivedMsg(NULL, NULL), DC_E_FAIL);
    TEST_ASSERT_EQUAL(dcGetReceivedMsg(&d, NULL), DC_E_FAIL);
    TEST_ASSERT_EQUAL(dcGetReceivedMsg(NULL, buffer), DC_E_FAIL);
}


void test_GetMessageNoMessagePending() {
    DeadcomL2 d;

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(res, DC_OK);

    uint8_t buffer[47];

    TEST_ASSERT_EQUAL(dcGetReceivedMsg(&d, buffer), DC_E_NOMSG);

    // Mutex should have been locked and unlocked
    TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 1);
    TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 1);
}


void test_GetMessage() {
    DeadcomL2 d;

    unsigned int recv;
    for (recv = 0; recv <= 7; recv++) {
        RESET_FAKE(mutexLock);
        RESET_FAKE(mutexUnlock);
        RESET_FAKE(transmitBytes);

        // Initialize the lib
        DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
        TEST_ASSERT_EQUAL(res, DC_OK);

        void transmit_bytes_fake_impl(uint8_t *data, uint8_t data_len) {
            // Nothing but send confirmation should have been sent
            TEST_ASSERT_EQUAL(((yahdlc_control_t*)data)->frame, YAHDLC_FRAME_ACK);
            TEST_ASSERT_EQUAL(((yahdlc_control_t*)data)->recv_seq_no, recv);
        }
        transmitBytes_fake.custom_fake = &transmit_bytes_fake_impl;

        // Simulate that we've received a message
        d.extractionBufferSize = 2;
        uint8_t orig_message[] = {0x42, 0x47};
        memcpy(d.extractionBuffer, orig_message, 2);
        d.recv_number = recv;

        int16_t received_msg_size = dcGetReceivedMsgLen(&d);
        TEST_ASSERT_EQUAL(received_msg_size, 2);

        uint8_t received_msg[received_msg_size];
        int16_t copied_size = dcGetReceivedMsg(&d, received_msg);

        TEST_ASSERT_EQUAL(copied_size, 2);
        TEST_ASSERT_EQUAL_MEMORY(d.extractionBuffer, orig_message, 2);

        // Mutex should have been locked and unlocked twice
        TEST_ASSERT_EQUAL(mutexLock_fake.call_count, 2);
        TEST_ASSERT_EQUAL(mutexUnlock_fake.call_count, 2);

        // Acknowledgment should have been transmitted exactly once
        TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, 1);
    }
}
