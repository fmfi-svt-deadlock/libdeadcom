#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "yahdlc.h"

/*
 * These are added tests. These verify behaviour of features added by us
 */


void test_ConnFrameControlField() {
    int ret;
    uint8_t frame_data[8], recv_data[8];
    size_t frame_length = 0, recv_length = 0;
    yahdlc_control_t control_send, control_recv;
    yahdlc_state_t state;

    yahdlc_reset_state(&state, 1024);
    // Initialize the control field structure with frame type and sequence number
    control_send.frame = YAHDLC_FRAME_CONN;

    // Create an empty frame with the control field information
    ret = yahdlc_frame_data(&control_send, NULL, 0, frame_data, &frame_length);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Get the data from the frame
    ret = yahdlc_get_data(&state, &control_recv, frame_data, frame_length, recv_data, &recv_length);

    // Result should be frame_length minus start flag to be discarded and no bytes received
    TEST_ASSERT_EQUAL_INT(ret, ((int )frame_length - 1));
    TEST_ASSERT_EQUAL_INT(0, recv_length);

    // Verify the control field information
    TEST_ASSERT_EQUAL_INT(control_recv.frame, control_send.frame);
    // Sequence number verification skipped, since U-frames don't carry one
}


void test_ConnAckFrameControlField() {
    int ret;
    uint8_t frame_data[8], recv_data[8];
    size_t frame_length = 0, recv_length = 0;
    yahdlc_control_t control_send, control_recv;
    yahdlc_state_t state;

    yahdlc_reset_state(&state, 1024);
    // Initialize the control field structure with frame type and sequence number
    control_send.frame = YAHDLC_FRAME_CONN_ACK;

    // Create an empty frame with the control field information
    ret = yahdlc_frame_data(&control_send, NULL, 0, frame_data, &frame_length);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Get the data from the frame
    ret = yahdlc_get_data(&state, &control_recv, frame_data, frame_length, recv_data, &recv_length);

    // Result should be frame_length minus start flag to be discarded and no bytes received
    TEST_ASSERT_EQUAL_INT(ret, ((int )frame_length - 1));
    TEST_ASSERT_EQUAL_INT(0, recv_length);

    // Verify the control field information
    TEST_ASSERT_EQUAL_INT(control_recv.frame, control_send.frame);
    // Sequence number verification skipped, since U-frames don't carry one
}


void test_GetDataOfTooBigFrame() {
    int ret;
    size_t frame_length, recv_length;
    yahdlc_control_t control_send = {}, control_recv = {};
    uint8_t send_data[512] = {0}, frame_data[520], recv_data[520];
    memset(recv_data, 0x55, 520);
    yahdlc_state_t state;

    yahdlc_reset_state(&state, 128);
    // Initialize data to be send with random values (up to 0x70 to keep below the values to be
    // escaped)
    for (size_t i = 0; i < sizeof(send_data); i++) {
        send_data[i] = 0x35;
    }

    // Initialize control field structure and create frame
    control_send.frame = YAHDLC_FRAME_DATA;

    ret = yahdlc_frame_data(&control_send, send_data, 512, frame_data, &frame_length);
    TEST_ASSERT_EQUAL_INT(0, ret);

    recv_length = 0;

    // Get the data from the frame
    ret = yahdlc_get_data(&state, &control_recv, frame_data, frame_length, recv_data, &recv_length);

    // IO error should have been returned, because the frame is too large.
    TEST_ASSERT_EQUAL_INT(-EIO, ret);

    // No bytes after byte 127 should have been overwritten
    uint8_t expected[520] = {0x55};
    memset(expected, 0x55, 520);
    TEST_ASSERT_EQUAL_MEMORY(expected, (recv_data+128), (520-128));
}


void test_GetDataOfTooBigFrameMultipleBuffers() {
    int ret;
    size_t frame_length, recv_length;
    yahdlc_control_t control_send = {}, control_recv = {};
    uint8_t send_data[512] = {0}, frame_data[520], recv_data[520];
    memset(recv_data, 0x55, 520);
    yahdlc_state_t state;

    yahdlc_reset_state(&state, 128);
    // Initialize data to be send with random values (up to 0x70 to keep below the values to be
    // escaped)
    for (size_t i = 0; i < sizeof(send_data); i++) {
        send_data[i] = 0x35;
    }

    // Initialize control field structure and create frame
    control_send.frame = YAHDLC_FRAME_DATA;

    ret = yahdlc_frame_data(&control_send, send_data, 512, frame_data, &frame_length);
    TEST_ASSERT_EQUAL_INT(0, ret);

    recv_length = 0;

    // Run though the different buffers (simulating decode of buffers from UART)
    for (size_t i = 0; i <= sizeof(send_data); i += 1) {
        // Decode the data
        ret = yahdlc_get_data(&state, &control_recv, &frame_data[i], 1, recv_data,
                              &recv_length);

        // All chunks should return either -ENOMSG or -EIO
        TEST_ASSERT((ret == -ENOMSG) || (ret == -EIO));
    }

    // No bytes after byte 127 should have been overwritten
    uint8_t expected[520];
    memset(expected, 0x55, 520);
    TEST_ASSERT_EQUAL_MEMORY(expected, recv_data+128, 520-128);
}
