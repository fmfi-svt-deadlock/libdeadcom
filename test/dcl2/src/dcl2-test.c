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


/* == Library initialization =====================================================================*/

void test_ValidInit() {
    DeadcomL2 d;

    DeadcomL2ThreadingVMT t = {
        &mutexInit,
        &mutexLock,
        &mutexUnlock,
        &condvarInit,
        &condvarWait,
        &condvarSignal
    };

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
