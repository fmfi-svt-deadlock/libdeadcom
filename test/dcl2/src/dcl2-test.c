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
