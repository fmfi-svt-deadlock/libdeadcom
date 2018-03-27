#include <string.h>
#include "unity.h"
#include "fff.h"

#include "dcl2.h"
#include "dcl2-fakes.c"

/*
 * Tests of Deadcom Layer 2 library, part 1: Initializing and transmitting frames
 */

void setUp(void) {
    FFF_FAKES_LIST(RESET_FAKE);
    FFF_RESET_HISTORY();
}

/* == Library initialization =====================================================================*/

void test_ValidInit() {
    DeadcomL2 d;
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);

    TEST_ASSERT_EQUAL(DC_OK, res);
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
    TEST_ASSERT_EQUAL(&transmitBytes, d.transmitBytes);
    TEST_ASSERT_EQUAL(&t, d.t);
    TEST_ASSERT_EQUAL(1, d.mutex_p);
    TEST_ASSERT_EQUAL(2, d.condvar_p);
    TEST_ASSERT_EQUAL(1, mutexInit_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexInit_fake.arg0_val);
    TEST_ASSERT_EQUAL(1, condvarInit_fake.call_count);
    TEST_ASSERT_EQUAL(2, condvarInit_fake.arg0_val);
}


void test_InvalidInit() {
    DeadcomL2Result res = dcInit(NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(DC_FAILURE, res);
}


/* == Connection establishment ===================================================================*/

void test_InvalidConnectionParams() {
    DeadcomL2Result res = dcConnect(NULL);
    TEST_ASSERT_EQUAL(DC_FAILURE, res);
}


void test_ValidConnection() {
    DeadcomL2 d;

    int frame_data_fake_impl(yahdlc_control_t *control, const char *data, unsigned int data_len,
                        char *data_out, unsigned int *data_out_len) {
        // this function should request creation of CONN frames only
        TEST_ASSERT_EQUAL(YAHDLC_FRAME_CONN, control->frame);
        // with no data
        TEST_ASSERT_EQUAL(0, data_len);
        // Lets return fake frame
        if (data_out) {
            data_out[0] = 0x42;
            data_out[1] = 0x47;
        }
        *data_out_len = 2;
    }
    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    void transmitBytes_fake_impl(uint8_t *data, uint8_t len) {
        // this function should onle ever attempt to transmit fake frame generated above
        TEST_ASSERT_EQUAL(2, len);
        uint8_t fake_frame[] = {0x42, 0x47};
        TEST_ASSERT_EQUAL_MEMORY(fake_frame, data, 2);
    }
    transmitBytes_fake.custom_fake = &transmitBytes_fake_impl;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // The condvarWait should return `true`, thereby simulating reception of connection ack
    SET_RETURN_SEQ(condvarWait, (bool[1]){true}, 1);

    // Simulate connection attempt
    res = dcConnect(&d);

    // Under these circumstances the connection attemt should have succeeded
    TEST_ASSERT_EQUAL(DC_OK, res);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.arg0_val);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.arg0_val);
    // The function should have waited on condition variable exactly once
    TEST_ASSERT_EQUAL(1, condvarWait_fake.call_count);
    TEST_ASSERT_EQUAL(2, condvarWait_fake.arg0_val);
    // Status should be connected
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
}


void test_ConnectionNoResponse() {
    // Frame construction and transmission is already verified in `test_ValidConnection`. Here we
    // verify only what happens when we won't receive a response.
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // The condvarWait should return `false`, thereby simulating no connection request ack
    SET_RETURN_SEQ(condvarWait, (bool[1]){false}, 1);
    // Simulate connection attempt
    res = dcConnect(&d);

    // Under these circumstances the connection attemt should have failed
    TEST_ASSERT_EQUAL(DC_NOT_CONNECTED, res);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.arg0_val);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.arg0_val);
    // The function should have waited on condition variable exactly once
    TEST_ASSERT_EQUAL(1, condvarWait_fake.call_count);
    TEST_ASSERT_EQUAL(2, condvarWait_fake.arg0_val);
    // Status should be connected
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
}


void test_ConnectionNoOpWhenConnected() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // The condvarWait should return `true`, thereby simulating reception of connection ack
    SET_RETURN_SEQ(condvarWait, (bool[1]){true}, 1);
    // Simulate first connection attempt
    res = dcConnect(&d);
    // which should be successfull
    TEST_ASSERT_EQUAL(DC_OK, res);
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);

    // and now try calling the connect function again
    res = dcConnect(&d);
    // return should be success and state connected
    TEST_ASSERT_EQUAL(DC_OK, res);
    TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
    // however functions should have been called only once
    TEST_ASSERT_EQUAL(1, condvarWait_fake.call_count);
    TEST_ASSERT_EQUAL(1, transmitBytes_fake.call_count);
}


void test_ConnectionFailWhenAlreadyConnecting() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // Simulate DC_CONNECTING state
    d.state = DC_CONNECTING;

    // The condvarWait should return `true`, thereby simulating reception of connection ack
    // if ever called
    SET_RETURN_SEQ(condvarWait, (bool[1]){true}, 1);
    res = dcConnect(&d);
    // return should be failure
    TEST_ASSERT_EQUAL(DC_FAILURE, res);
    // and no frame / transmit / wait functions should have been called
    TEST_ASSERT_EQUAL(0, condvarWait_fake.call_count);
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
    TEST_ASSERT_EQUAL(0, yahdlc_frame_data_fake.call_count);
}


/* == Dropping connection ========================================================================*/

void test_disconnectWhenDisconnected() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // Attempt to disconnect already disconnected link
    res = dcDisconnect(&d);

    // result should be OK
    TEST_ASSERT_EQUAL(DC_OK, res);
    // State should be disconnected
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
    // Mutex should be locked / unlocked exactly once
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
}


void test_disconnectWhenConnecting() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // Simulate connecting link
    d.state = DC_CONNECTING;

    // Attempt to disconnect connecting link
    res = dcDisconnect(&d);

    // result should be failure
    TEST_ASSERT_EQUAL(DC_FAILURE, res);
    // state should be unchanged
    TEST_ASSERT_EQUAL(DC_CONNECTING, d.state);
    // mutex should be locked / unlocked exactly once
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
}


void test_disconnectWhenConnected() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // Simulate connected link
    d.state = DC_CONNECTED;

    // Attempt to disconnect already disconnected link
    res = dcDisconnect(&d);

    // result should be OK
    TEST_ASSERT_EQUAL(DC_OK, res);
    // State should be disconnected
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
    // Mutex should be locked / unlocked exactly once
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
}



/* == Sending a message ==========================================================================*/

void test_SendMessageInvalidParams() {
    DeadcomL2 d;

    const uint8_t message[] = {0x42, 0x47};

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    res = dcSendMessage(NULL, message, sizeof(message));
    // failed call, no locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(DC_FAILURE, res);
    TEST_ASSERT_EQUAL(0, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);

    res = dcSendMessage(&d, NULL, sizeof(message));
    // failed call, no locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(DC_FAILURE, res);
    TEST_ASSERT_EQUAL(0, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);

    res = dcSendMessage(&d, message, 0);
    // failed call, no locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(DC_FAILURE, res);
    TEST_ASSERT_EQUAL(0, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);

    res = dcSendMessage(NULL, message, DEADCOM_PAYLOAD_MAX_LEN+1);
    // failed call, no locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(DC_FAILURE, res);
    TEST_ASSERT_EQUAL(0, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
}


void test_SendMessageInvalidState() {
    DeadcomL2 d;

    const uint8_t message[] = {0x42, 0x47};

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // Disconnected state
    res = dcSendMessage(&d, message, sizeof(message));
    // failed call, one of each locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(DC_NOT_CONNECTED, res);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    d.state = DC_CONNECTING;
    // Connecting state
    res = dcSendMessage(&d, message, sizeof(message));
    // failed call, one of each locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(DC_NOT_CONNECTED, res);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);

    RESET_FAKE(mutexLock);
    RESET_FAKE(mutexUnlock);

    d.state = DC_TRANSMITTING;
    // Connected but transmitting
    res = dcSendMessage(&d, message, sizeof(message));
    // failed call, one of each locking calls, no attempt to transmit
    TEST_ASSERT_EQUAL(DC_FAILURE, res);
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(0, transmitBytes_fake.call_count);
}


void test_SendMessageWithAcknowledgment() {
    DeadcomL2 d;

    unsigned int sseq, rseq;
    for (sseq = 0; sseq <= 7; sseq++) {
        for (rseq = 0; rseq <= 7; rseq++) {
            FFF_FAKES_LIST(RESET_FAKE);
            FFF_RESET_HISTORY();

            yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

            const uint8_t message[] = {0x42, 0x47};

            bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
                // Fake implementation of condvarWait function.
                // This function should be called after the message is transmitted.
                // The mutex should be locked
                TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
                TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
                // Bytes should have been transmitted
                TEST_ASSERT_EQUAL(1, transmitBytes_fake.call_count);
                // Library status should be 'transmitting'
                TEST_ASSERT_EQUAL(DC_TRANSMITTING, d.state);

                // Verify that proper message has been transmitted
                uint8_t *txed_bytes = transmitBytes_fake.arg0_val;
                unsigned int num_txed_bytes = transmitBytes_fake.arg1_val;

                // Verify that this is a DATA frame with correct sequence numbers
                TEST_ASSERT_EQUAL(YAHDLC_FRAME_DATA, ((yahdlc_control_t*)txed_bytes)->frame);
                TEST_ASSERT_EQUAL(sseq, ((yahdlc_control_t*)txed_bytes)->send_seq_no);
                TEST_ASSERT_EQUAL(rseq, ((yahdlc_control_t*)txed_bytes)->recv_seq_no);

                // Verify that we've sent 2 bytes
                TEST_ASSERT_EQUAL(2, txed_bytes[sizeof(yahdlc_control_t)+3]);
                // Verify that we've sent the correct message
                TEST_ASSERT_EQUAL_MEMORY(message, (txed_bytes + sizeof(yahdlc_control_t) + 4), 2);

                // The other station acknowledged
                d.last_response = DC_RESP_OK;
                return true;
            }
            condvarWait_fake.custom_fake = &condvarWait_fakeimpl;

            // Initialize the lib
            DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
            TEST_ASSERT_EQUAL(DC_OK, res);

            // Simulate connected state
            d.state = DC_CONNECTED;
            d.send_number = sseq;
            d.next_expected_ack  = sseq;
            d.recv_number = rseq;

            res = dcSendMessage(&d, message, sizeof(message));
            // Transmission should be successful
            TEST_ASSERT_EQUAL(DC_OK, res);
            // Mutex should have been locked and unlocked exactly once
            TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
            TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
            // We should have waited on condition variable exactly once
            TEST_ASSERT_EQUAL(1, condvarWait_fake.call_count);
            // Library status should be 'connected'
            TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
        }
    }
}


void test_SendMessageWhenNacked() {
    DeadcomL2 d;

    unsigned int nack_number;
    for (nack_number = 1; nack_number < DEADCOM_MAX_FAILURE_COUNT; nack_number++) {
        FFF_FAKES_LIST(RESET_FAKE);
        FFF_RESET_HISTORY();

        yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

        const uint8_t message[] = {0x42, 0x47};

        unsigned int nacked_times = 0;
        bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
            // Fake implementation of condvarWait function.
            // This function should be called after the message is transmitted.
            // The mutex should be locked
            TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
            TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
            // Bytes should have been transmitted
            TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, (nacked_times+1));
            // Library status should be 'transmitting'
            TEST_ASSERT_EQUAL(DC_TRANSMITTING, d.state);

            // Verify that proper message has been transmitted
            uint8_t *txed_bytes = transmitBytes_fake.arg0_val;
            unsigned int num_txed_bytes = transmitBytes_fake.arg1_val;

            // Verify that this is a DATA frame with correct sequence numbers
            TEST_ASSERT_EQUAL(YAHDLC_FRAME_DATA, ((yahdlc_control_t*)txed_bytes)->frame);
            TEST_ASSERT_EQUAL(0, ((yahdlc_control_t*)txed_bytes)->send_seq_no);
            TEST_ASSERT_EQUAL(0, ((yahdlc_control_t*)txed_bytes)->recv_seq_no);

            // Verify that we've sent 2 bytes
            TEST_ASSERT_EQUAL(2, txed_bytes[sizeof(yahdlc_control_t)+3]);
            // Verify that we've sent the correct message
            TEST_ASSERT_EQUAL_MEMORY(message, (txed_bytes + sizeof(yahdlc_control_t) + 4), 2);

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
        TEST_ASSERT_EQUAL(DC_OK, res);

        // Simulate connected state
        d.state = DC_CONNECTED;

        res = dcSendMessage(&d, message, sizeof(message));
        // Transmission should be successful
        TEST_ASSERT_EQUAL(DC_OK, res);
        // Mutex should have been locked and unlocked exactly once
        TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
        TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
        // We should have waited on condition variable after each nack and once more for good send
        TEST_ASSERT_EQUAL(condvarWait_fake.call_count, (nack_number+1));
        // Library status should be 'connected'
        TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
    }
}


void test_SendMessageWhenTimeout() {
    DeadcomL2 d;

    unsigned int timeout_number;
    for (timeout_number = 1; timeout_number < DEADCOM_MAX_FAILURE_COUNT; timeout_number++) {
        FFF_FAKES_LIST(RESET_FAKE);
        FFF_RESET_HISTORY();

        yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

        const uint8_t message[] = {0x42, 0x47};

        unsigned int timeout_times = 0;
        bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
            // Fake implementation of condvarWait function.
            // This function should be called after the message is transmitted.
            // The mutex should be locked
            TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
            TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
            // Bytes should have been transmitted
            TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, (timeout_times+1));
            // Library status should be 'transmitting'
            TEST_ASSERT_EQUAL(DC_TRANSMITTING, d.state);

            // Verify that proper message has been transmitted
            uint8_t *txed_bytes = transmitBytes_fake.arg0_val;
            unsigned int num_txed_bytes = transmitBytes_fake.arg1_val;

            // Verify that this is a DATA frame with correct sequence numbers
            TEST_ASSERT_EQUAL(YAHDLC_FRAME_DATA, ((yahdlc_control_t*)txed_bytes)->frame);
            TEST_ASSERT_EQUAL(0, ((yahdlc_control_t*)txed_bytes)->send_seq_no);
            TEST_ASSERT_EQUAL(0, ((yahdlc_control_t*)txed_bytes)->recv_seq_no);

            // Verify that we've sent 2 bytes
            TEST_ASSERT_EQUAL(2, txed_bytes[sizeof(yahdlc_control_t)+3]);
            // Verify that we've sent the correct message
            TEST_ASSERT_EQUAL_MEMORY(message, (txed_bytes + sizeof(yahdlc_control_t) + 4), 2);

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
        TEST_ASSERT_EQUAL(DC_OK, res);

        // Simulate connected state
        d.state = DC_CONNECTED;

        res = dcSendMessage(&d, message, sizeof(message));
        // Transmission should be successful
        TEST_ASSERT_EQUAL(DC_OK, res);
        // Mutex should have been locked and unlocked exactly once
        TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
        TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
        // We should have waited on condition variable after each nack and once more for good send
        TEST_ASSERT_EQUAL(condvarWait_fake.call_count, (timeout_number+1));
        // Library status should be 'connected'
        TEST_ASSERT_EQUAL(DC_CONNECTED, d.state);
    }
}


void test_SendMessageFailTooManyNacks() {
    DeadcomL2 d;

    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    const uint8_t message[] = {0x42, 0x47};

    unsigned int timeout_times = 0;
    bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
        // Fake implementation of condvarWait function.
        // This function should be called after the message is transmitted.
        // The mutex should be locked
        TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
        TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
        // Bytes should have been transmitted
        TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, (timeout_times+1));
        // Library status should be 'transmitting'
        TEST_ASSERT_EQUAL(DC_TRANSMITTING, d.state);

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
    TEST_ASSERT_EQUAL(DC_OK, res);

    // Simulate connected state
    d.state = DC_CONNECTED;

    res = dcSendMessage(&d, message, sizeof(message));
    // Transmission should fail with link reset error
    TEST_ASSERT_EQUAL(DC_LINK_RESET, res);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DEADCOM_MAX_FAILURE_COUNT, condvarWait_fake.call_count);
    // Library status should be 'disconnected'
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
}


void test_SendMessageFailTooManyTimeouts() {
    DeadcomL2 d;

    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    const uint8_t message[] = {0x42, 0x47};

    unsigned int timeout_times = 0;
    bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
        // Fake implementation of condvarWait function.
        // This function should be called after the message is transmitted.
        // The mutex should be locked
        TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
        TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
        // Bytes should have been transmitted
        TEST_ASSERT_EQUAL(transmitBytes_fake.call_count, (timeout_times+1));
        // Library status should be 'transmitting'
        TEST_ASSERT_EQUAL(DC_TRANSMITTING, d.state);

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
    TEST_ASSERT_EQUAL(DC_OK, res);

    // Simulate connected state
    d.state = DC_CONNECTED;

    res = dcSendMessage(&d, message, sizeof(message));
    // Transmission should fail with link reset error
    TEST_ASSERT_EQUAL(DC_LINK_RESET, res);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(DEADCOM_MAX_FAILURE_COUNT, condvarWait_fake.call_count);
    // Library status should be 'disconnected'
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
}


void test_SendMessageOtherStationDropsLink() {
    DeadcomL2 d;

    yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

    const uint8_t message[] = {0x42, 0x47};

    bool condvarWait_fakeimpl(void* condvar, unsigned int timeout) {
        // Fake implementation of condvarWait function.
        // This function should be called after the message is transmitted.
        // The mutex should be locked
        TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
        TEST_ASSERT_EQUAL(0, mutexUnlock_fake.call_count);
        // Bytes should have been transmitted
        TEST_ASSERT_EQUAL(1, transmitBytes_fake.call_count);
        // Library status should be 'transmitting'
        TEST_ASSERT_EQUAL(DC_TRANSMITTING, d.state);

        d.last_response = DC_RESP_NOLINK;
        return true;
    }
    condvarWait_fake.custom_fake = &condvarWait_fakeimpl;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // Simulate connected state
    d.state = DC_CONNECTED;

    res = dcSendMessage(&d, message, sizeof(message));
    // Transmission should fail with link reset error
    TEST_ASSERT_EQUAL(DC_LINK_RESET, res);
    // Mutex should have been locked and unlocked exactly once
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
    TEST_ASSERT_EQUAL(1, condvarWait_fake.call_count);
    // Library status should be 'disconnected'
    TEST_ASSERT_EQUAL(DC_DISCONNECTED, d.state);
}


/* == Getting previously received message ========================================================*/

void test_GetMessageLengthInvalidParams() {
    TEST_ASSERT_EQUAL(DC_E_FAIL, dcGetReceivedMsgLen(NULL));
}


void test_GetMessageLengthNoMessagePending() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    TEST_ASSERT_EQUAL(DC_E_NOMSG, dcGetReceivedMsgLen(&d));

    // Mutex should have been locked and unlocked
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
}


void test_GetMessageLength() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    // Simulate that we've received a message
    d.extractionBufferSize = 47;
    d.extractionComplete = true;

    TEST_ASSERT_EQUAL(47, dcGetReceivedMsgLen(&d));

    // Mutex should have been locked and unlocked
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
}


void test_GetMessageInvalidParams() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    uint8_t buffer[47];

    TEST_ASSERT_EQUAL(dcGetReceivedMsg(NULL, NULL), DC_E_FAIL);
    TEST_ASSERT_EQUAL(dcGetReceivedMsg(&d, NULL), DC_E_FAIL);
    TEST_ASSERT_EQUAL(dcGetReceivedMsg(NULL, buffer), DC_E_FAIL);
}


void test_GetMessageNoMessagePending() {
    DeadcomL2 d;

    // Initialize the lib
    DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
    TEST_ASSERT_EQUAL(DC_OK, res);

    uint8_t buffer[47];

    TEST_ASSERT_EQUAL(dcGetReceivedMsg(&d, buffer), DC_E_NOMSG);

    // Mutex should have been locked and unlocked
    TEST_ASSERT_EQUAL(1, mutexLock_fake.call_count);
    TEST_ASSERT_EQUAL(1, mutexUnlock_fake.call_count);
}


void test_GetMessage() {
    DeadcomL2 d;

    unsigned int recv;
    for (recv = 0; recv <= 7; recv++) {
        FFF_FAKES_LIST(RESET_FAKE);
        FFF_RESET_HISTORY();
        yahdlc_frame_data_fake.custom_fake = &frame_data_fake_impl;

        // Initialize the lib
        DeadcomL2Result res = dcInit(&d, (void*)1, (void*)2, &t, &transmitBytes);
        TEST_ASSERT_EQUAL(DC_OK, res);

        void transmit_bytes_fake_impl(uint8_t *data, uint8_t data_len) {
            // Nothing but send confirmation should have been sent
            TEST_ASSERT_EQUAL(YAHDLC_FRAME_ACK, ((yahdlc_control_t*)data)->frame);
            TEST_ASSERT_EQUAL(recv, ((yahdlc_control_t*)data)->recv_seq_no);
        }
        transmitBytes_fake.custom_fake = &transmit_bytes_fake_impl;

        // Simulate that we've received a message
        d.extractionBufferSize = 2;
        d.extractionComplete = true;
        uint8_t orig_message[] = {0x42, 0x47};
        memcpy(d.extractionBuffer, orig_message, 2);
        d.recv_number = recv;

        int16_t received_msg_size = dcGetReceivedMsgLen(&d);
        TEST_ASSERT_EQUAL(2, received_msg_size);

        uint8_t received_msg[received_msg_size];
        int16_t copied_size = dcGetReceivedMsg(&d, received_msg);

        TEST_ASSERT_EQUAL(2, copied_size);
        TEST_ASSERT_EQUAL_MEMORY(d.extractionBuffer, orig_message, 2);

        // Mutex should have been locked and unlocked twice
        TEST_ASSERT_EQUAL(2, mutexLock_fake.call_count);
        TEST_ASSERT_EQUAL(2, mutexUnlock_fake.call_count);

        // Acknowledgment should have been transmitted exactly once
        TEST_ASSERT_EQUAL(1, transmitBytes_fake.call_count);
    }
}
