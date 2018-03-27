#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "yahdlc.h"

/*
 * These are added tests. These verify behaviour of features added by us
 */


void test_ConnFrameControlField() {
    int ret;
    char frame_data[8], recv_data[8];
    unsigned int i, frame_length = 0, recv_length = 0;
    yahdlc_control_t control_send, control_recv;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Initialize the control field structure with frame type and sequence number
    control_send.frame = YAHDLC_FRAME_CONN;

    // Create an empty frame with the control field information
    ret = yahdlc_frame_data(&control_send, NULL, 0, frame_data, &frame_length);
    TEST_ASSERT_EQUAL_INT(ret, 0);

    // Get the data from the frame
    ret = yahdlc_get_data(&state, &control_recv, frame_data, frame_length, recv_data, &recv_length);

    // Result should be frame_length minus start flag to be discarded and no bytes received
    TEST_ASSERT_EQUAL_INT(ret, ((int )frame_length - 1));
    TEST_ASSERT_EQUAL_INT(recv_length, 0);

    // Verify the control field information
    TEST_ASSERT_EQUAL_INT(control_send.frame, control_recv.frame);
    // Sequence number verification skipped, since U-frames don't carry one
}


void test_ConnAckFrameControlField() {
    int ret;
    char frame_data[8], recv_data[8];
    unsigned int i, frame_length = 0, recv_length = 0;
    yahdlc_control_t control_send, control_recv;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Initialize the control field structure with frame type and sequence number
    control_send.frame = YAHDLC_FRAME_CONN_ACK;

    // Create an empty frame with the control field information
    ret = yahdlc_frame_data(&control_send, NULL, 0, frame_data, &frame_length);
    TEST_ASSERT_EQUAL_INT(ret, 0);

    // Get the data from the frame
    ret = yahdlc_get_data(&state, &control_recv, frame_data, frame_length, recv_data, &recv_length);

    // Result should be frame_length minus start flag to be discarded and no bytes received
    TEST_ASSERT_EQUAL_INT(ret, ((int )frame_length - 1));
    TEST_ASSERT_EQUAL_INT(recv_length, 0);

    // Verify the control field information
    TEST_ASSERT_EQUAL_INT(control_send.frame, control_recv.frame);
    // Sequence number verification skipped, since U-frames don't carry one
}
