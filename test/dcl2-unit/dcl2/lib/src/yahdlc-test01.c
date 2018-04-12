#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "yahdlc.h"

/*
 * These tests are from the original yahdlc project. They have been ported to Unity and irrelevant
 * parts were removed to reflect our changes.
 */

void test_FrameDataInvalidInputs() {
    int ret;
    yahdlc_control_t control = {};
    size_t frame_length = 0;
    uint8_t send_data[8] = {0}, frame_data[8];

    // Check invalid control field parameter
    ret = yahdlc_frame_data(NULL, send_data, sizeof(send_data), frame_data, &frame_length);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);

    // Check invalid source buffer parameter (positive source buffer length)
    ret = yahdlc_frame_data(&control, NULL, 1, frame_data, &frame_length);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);

    // Check that it is possible to create an empty frame
    ret = yahdlc_frame_data(&control, NULL, 0, frame_data, &frame_length);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Check invalid destination buffer parameter (should compute only frame length)
    ret = yahdlc_frame_data(&control, send_data, sizeof(send_data), NULL, &frame_length);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Check invalid destination buffer length parameter
    ret = yahdlc_frame_data(&control, send_data, sizeof(send_data), frame_data, NULL);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}


void test_GetDataInvalidInputs() {
    int ret;
    yahdlc_control_t control;
    size_t recv_length = 0;
    uint8_t frame_data[8], recv_data[8];

    ret = yahdlc_get_data(NULL, &control, frame_data, sizeof(frame_data), recv_data,
                          &recv_length);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}


void test_DataFrameControlField() {
    int ret;
    uint8_t frame_data[8], recv_data[8];
    size_t i, j, frame_length = 0, recv_length = 0;
    yahdlc_control_t control_send, control_recv;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Run through the supported sequence numbers (3-bit)
    for (i = 0; i <= 7; i++) {
        for (j = 0; j <= 7; j++) {
            // Initialize the control field structure with frame type and sequence number
            control_send.frame = YAHDLC_FRAME_DATA;
            control_send.send_seq_no = i;
            control_send.recv_seq_no = j;

            // Create an empty frame with the control field information
            ret = yahdlc_frame_data(&control_send, NULL, 0, frame_data, &frame_length);
            TEST_ASSERT_EQUAL_INT(0, ret);

            recv_length = 0;
            // Get the data from the frame
            ret = yahdlc_get_data(&state, &control_recv, frame_data, frame_length, recv_data,
                                  &recv_length);

            // Result should be frame_length minus start flag to be discarded and no bytes received
            TEST_ASSERT_EQUAL_INT(ret, ((int )frame_length - 1));
            TEST_ASSERT_EQUAL_INT(0, recv_length);

            // Verify the control field information
            TEST_ASSERT_EQUAL_INT(control_recv.frame, control_send.frame);
            TEST_ASSERT_EQUAL_INT(control_recv.send_seq_no, control_send.send_seq_no);
            TEST_ASSERT_EQUAL_INT(control_recv.recv_seq_no, control_send.recv_seq_no);
        }
    }
}


void test_AckFrameControlField() {
    int ret;
    uint8_t frame_data[8], recv_data[8];
    size_t i, frame_length = 0, recv_length = 0;
    yahdlc_control_t control_send, control_recv;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Run through the supported sequence numbers (3-bit)
    for (i = 0; i <= 7; i++) {
      // Initialize the control field structure with frame type and sequence number
      control_send.frame = YAHDLC_FRAME_ACK;
      control_send.recv_seq_no = i;

      // Create an empty frame with the control field information
      ret = yahdlc_frame_data(&control_send, NULL, 0, frame_data, &frame_length);
      TEST_ASSERT_EQUAL_INT(0, ret);

      // Get the data from the frame
      ret = yahdlc_get_data(&state, &control_recv, frame_data, frame_length, recv_data,
                            &recv_length);

      // Result should be frame_length minus start flag to be discarded and no bytes received
      TEST_ASSERT_EQUAL_INT(ret, ((int )frame_length - 1));
      TEST_ASSERT_EQUAL_INT(0, recv_length);

      // Verify the control field information
      TEST_ASSERT_EQUAL_INT(control_recv.frame, control_send.frame);
      TEST_ASSERT_EQUAL_INT(control_recv.recv_seq_no, control_send.recv_seq_no);
    }
}


void test_NackFrameControlField() {
    int ret;
    uint8_t frame_data[8], recv_data[8];
    size_t i, frame_length = 0, recv_length = 0;
    yahdlc_control_t control_send, control_recv;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Run through the supported sequence numbers (3-bit)
    for (i = 0; i <= 7; i++) {
        // Initialize the control field structure with frame type and sequence number
        control_send.frame = YAHDLC_FRAME_NACK;
        control_send.recv_seq_no = i;

        // Create an empty frame with the control field information
        ret = yahdlc_frame_data(&control_send, NULL, 0, frame_data, &frame_length);
        TEST_ASSERT_EQUAL_INT(0, ret);

        // Get the data from the frame
        ret = yahdlc_get_data(&state, &control_recv, frame_data, frame_length, recv_data,
                              &recv_length);

        // Result should be frame_length minus start flag to be discarded and no bytes received
        TEST_ASSERT_EQUAL_INT(ret, ((int )frame_length - 1));
        TEST_ASSERT_EQUAL_INT(0, recv_length);

        // Verify the control field information
        TEST_ASSERT_EQUAL_INT(control_recv.frame, control_send.frame);
        TEST_ASSERT_EQUAL_INT(control_recv.recv_seq_no, control_send.recv_seq_no);
    }
}


void test_0To512BytesData() {
    int ret;
    size_t i, frame_length = 0, estimated_frame_length = 0, recv_length = 0;
    yahdlc_control_t control_send = {}, control_recv = {};
    uint8_t send_data[512] = {0}, frame_data[520], recv_data[520];
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Initialize data to be send with random values (up to 0x70 to keep below the values to be
    // escaped)
    for (i = 0; i < sizeof(send_data); i++) {
        send_data[i] = (uint8_t) (rand() % 0x70);
    }

    // Run through the different data sizes
    for (i = 0; i <= sizeof(send_data); i++) {
        // Initialize control field structure and create frame
        control_send.frame = YAHDLC_FRAME_DATA;

        ret = yahdlc_frame_data(&control_send, send_data, i, NULL,
                                &estimated_frame_length);
        TEST_ASSERT_EQUAL_INT(0, ret);

        ret = yahdlc_frame_data(&control_send, send_data, i, frame_data,
                                &frame_length);

        // Check that frame length is maximum 2 bytes larger than data due to escape of FCS value
        TEST_ASSERT(frame_length <= ((i + 6) + 2));
        TEST_ASSERT_EQUAL_INT(0, ret);
        TEST_ASSERT_EQUAL(estimated_frame_length, frame_length);

        recv_length = 0;

        // Get the data from the frame
        ret = yahdlc_get_data(&state, &control_recv, frame_data, frame_length, recv_data,
                              &recv_length);

        // Bytes to be discarded should be one byte less than frame length
        TEST_ASSERT_EQUAL_INT(ret, (frame_length - 1));
        TEST_ASSERT_EQUAL_INT(i, recv_length);

        // Compare the send and received bytes
        ret = memcmp(send_data, recv_data, i);
        TEST_ASSERT_EQUAL_INT(0, ret);
    }
}


void test_5BytesFrame() {
    int ret;
    size_t recv_length = 0;
    yahdlc_control_t control;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Create an invalid frame with only one byte of FCS
    uint8_t recv_data[8], frame_data[] = { YAHDLC_FLAG_SEQUENCE,
                                          (uint8_t) YAHDLC_ALL_STATION_ADDR, 0x10, 0x33,
                                          YAHDLC_FLAG_SEQUENCE };

    // Now decode the frame
    ret = yahdlc_get_data(&state, &control, frame_data, sizeof(frame_data), recv_data,
                          &recv_length);

    // Check that the decoded data will return invalid FCS error and 4 bytes to be discarded
    TEST_ASSERT_EQUAL_INT(-EIO, ret);
    TEST_ASSERT_EQUAL_INT(recv_length, (sizeof(frame_data) - 1));
}


void test_EndFlagSequenceInNewBuffer() {
    int ret;
    yahdlc_control_t control = {};
    uint8_t send_data[16] = {0}, frame_data[24] = {0}, recv_data[24];
    size_t i, frame_length = 0, recv_length = 0;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Initialize data to be send with random values (up to 0x70 to keep below the values to be escaped)
    for (i = 0; i < sizeof(send_data); i++) {
        send_data[i] = (uint8_t) (rand() % 0x70);
    }

    // Initialize control field structure and create frame
    control.frame = YAHDLC_FRAME_DATA;
    ret = yahdlc_frame_data(&control, send_data, sizeof(send_data), frame_data,
                            &frame_length);

    // Check that frame length is maximum 2 bytes larger than data due to escape of FCS value
    TEST_ASSERT(frame_length <= ((sizeof(send_data) + 6) + 2));
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Decode the data up to end flag sequence byte which should return no valid messages error
    ret = yahdlc_get_data(&state, &control, frame_data, frame_length - 1, recv_data,
                          &recv_length);
    TEST_ASSERT_EQUAL_INT(-ENOMSG, ret);
    TEST_ASSERT_EQUAL_INT(0, recv_length);

    // Now decode the end flag sequence byte which should result in a decoded frame
    ret = yahdlc_get_data(&state, &control, &frame_data[frame_length - 1], 1, recv_data,
                          &recv_length);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(recv_length, sizeof(send_data));

    // Make sure that the data is valid
    ret = memcmp(send_data, recv_data, sizeof(send_data));
    TEST_ASSERT_EQUAL_INT(0, ret);
}


void test_FlagSequenceAndControlEscapeInData() {
    int ret;
    yahdlc_control_t control = {};
    size_t frame_length = 0, recv_length = 0;
    uint8_t send_data[] = { YAHDLC_FLAG_SEQUENCE, 0x11, YAHDLC_CONTROL_ESCAPE },
                          frame_data[16], recv_data[16];
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Initialize control field structure and create frame
    control.frame = YAHDLC_FRAME_DATA;
    ret = yahdlc_frame_data(&control, send_data, sizeof(send_data), frame_data,
                            &frame_length);

    // Length should be frame size (6) + 3 data bytes + 2 escaped characters = 11
    TEST_ASSERT_EQUAL_INT(11, frame_length);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Decode the frame
    ret = yahdlc_get_data(&state, &control, frame_data, frame_length, recv_data,
                          &recv_length);
    TEST_ASSERT_EQUAL_INT(ret, (int )(frame_length - 1));
    TEST_ASSERT_EQUAL_INT(recv_length, sizeof(send_data));

    // Make sure that the data is valid
    ret = memcmp(send_data, recv_data, sizeof(send_data));
    TEST_ASSERT_EQUAL_INT(0, ret);
}


void test_getDataFromMultipleBuffers() {
    int ret;
    yahdlc_control_t control = {};
    uint8_t send_data[512] = {0}, frame_data[530] = {0}, recv_data[530];
    size_t i, frame_length = 0, recv_length = 0, buf_length = 16;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Initialize data to be send with random values (up to 0x70 to keep below the values to be escaped)
    for (i = 0; i < sizeof(send_data); i++) {
        send_data[i] = (uint8_t) (rand() % 0x70);
    }

    // Initialize control field structure and create frame
    control.frame = YAHDLC_FRAME_DATA;
    ret = yahdlc_frame_data(&control, send_data, sizeof(send_data), frame_data,
                            &frame_length);

    // Check that frame length is maximum 2 bytes larger than data due to escape of FCS value
    TEST_ASSERT(frame_length <= ((sizeof(send_data) + 6) + 2));
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Run though the different buffers (simulating decode of buffers from UART)
    for (i = 0; i <= sizeof(send_data); i += buf_length) {
        // Decode the data
        ret = yahdlc_get_data(&state, &control, &frame_data[i], buf_length, recv_data,
                              &recv_length);

        if (i < sizeof(send_data)) {
          // All chunks until the last should return no message and zero length
          TEST_ASSERT_EQUAL_INT(-ENOMSG, ret);
          TEST_ASSERT_EQUAL_INT(0, recv_length);
        } else {
          // The last chunk should return max 6 frame bytes - 1 start flag sequence byte + 2 byte
          // for the possible escaped FCS = 7 bytes
          TEST_ASSERT(ret <= 7);
          TEST_ASSERT_EQUAL_INT(recv_length, sizeof(send_data));
        }
    }

    // Make sure that the data is valid
    ret = memcmp(send_data, recv_data, sizeof(send_data));
    TEST_ASSERT_EQUAL_INT(0, ret);
}


void test_MultipleFramesWithSingleFlagSequence() {
    int ret, frame_index = 0;
    yahdlc_control_t control = {};
    uint8_t send_data[32] = {0}, frame_data[512], recv_data[512];
    size_t i, frame_length = 0, recv_length = 0, frames = 10;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Initialize data to be send with random values (up to 0x70 to keep below the values to be escaped)
    for (i = 0; i < sizeof(send_data); i++) {
        send_data[i] = (char) (rand() % 0x70);
    }

    // Run through the number of frames to be send
    for (i = 0; i < frames; i++) {
        // Initialize control field structure and create frame
        control.frame = YAHDLC_FRAME_DATA;
        ret = yahdlc_frame_data(&control, send_data, sizeof(send_data),
                                &frame_data[frame_index], &frame_length);

        // Check that frame length is maximum 2 bytes larger than data due to escape of FCS value
        TEST_ASSERT(frame_length <= ((sizeof(send_data) + 6) + 2));
        TEST_ASSERT_EQUAL_INT(0, ret);

        // Remove the end flag sequence byte as there must only be one flag sequence byte between frames
        frame_index += frame_length - 1;
    }

    // For the last frame we need the end flag sequence byte
    frame_length = frame_index + 1;
    frame_index = 0;

    // Now decode all the frames
    for (i = 0; i < frames; i++) {
        // Get the data from the frame
        ret = yahdlc_get_data(&state, &control, &frame_data[frame_index],
                              frame_length - frame_index, recv_data, &recv_length);

        // Check that frame length (minus 1 due to missing end byte sequence) to is max 2 bytes larger than data
        // due to escape of FCS value
        TEST_ASSERT(ret <= (int )((sizeof(send_data) + 5) + 2));
        TEST_ASSERT_EQUAL_INT(recv_length, sizeof(send_data));

        // Increment the number of bytes to be discarded from the frame data (source) buffer
        frame_index += ret;

        // Compare the send and received bytes
        ret = memcmp(send_data, recv_data, sizeof(send_data));
        TEST_ASSERT_EQUAL_INT(0, ret);
    }
}


void test_MultipleFramesWithDoubleFlagSequence() {
    int ret, frame_index = 0;
    yahdlc_control_t control = {};
    uint8_t send_data[32] = {0}, frame_data[512], recv_data[512];
    size_t i, frame_length = 0, recv_length = 0, frames = 10;
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Initialize data to be send with random values (up to 0x70 to keep below the values to be escaped)
    for (i = 0; i < sizeof(send_data); i++) {
        send_data[i] = (char) (rand() % 0x70);
    }

    // Run through the number of frames to be send
    for (i = 0; i < frames; i++) {
        // Initialize control field structure and create frame
        control.frame = YAHDLC_FRAME_DATA;
        ret = yahdlc_frame_data(&control, send_data, sizeof(send_data),
                                &frame_data[frame_index], &frame_length);

        // Check that frame length is maximum 2 bytes larger than data due to escape of FCS value
        TEST_ASSERT(frame_length <= ((sizeof(send_data) + 6) + 2));
        TEST_ASSERT_EQUAL_INT(0, ret);

        // Do not remove end flag sequence to test the silent discard of this additional byte
        frame_index += frame_length;
    }

    frame_length = frame_index;
    frame_index = 0;

    // Now decode all the frames
    for (i = 0; i < frames; i++) {
        // Get the data from the frame
        ret = yahdlc_get_data(&state, &control, &frame_data[frame_index],
                              frame_length - frame_index, recv_data, &recv_length);

        // Check that frame length is maximum 2 bytes larger than data due to escape of FCS value
        TEST_ASSERT(ret <= (int )((sizeof(send_data) + 6) + 2));
        TEST_ASSERT_EQUAL_INT(recv_length, sizeof(send_data));

        // Increment the number of bytes to be discarded from the frame data buffer
        frame_index += ret;

        // Compare the send and received bytes
        ret = memcmp(send_data, recv_data, sizeof(send_data));
        TEST_ASSERT_EQUAL_INT(0, ret);
    }
}


void test_FramesWithBitErrors() {
    int ret;
    yahdlc_control_t control = {};
    size_t i, frame_length = 0, recv_length = 0;
    uint8_t send_data[] = { 0x55 }, frame_data[8], recv_data[8];
    yahdlc_state_t state;

    yahdlc_reset_state(&state);
    // Run through the bytes in a frame with a single byte of data
    for (i = 0; i < (sizeof(send_data) + 6); i++) {
        // Initialize control field structure
        control.frame = YAHDLC_FRAME_DATA;

        // Create the frame
        ret = yahdlc_frame_data(&control, send_data, sizeof(send_data), frame_data,
                                &frame_length);
        TEST_ASSERT_EQUAL_INT(frame_length, (sizeof(send_data) + 6));
        TEST_ASSERT_EQUAL_INT(0, ret);

        // Generate a single bit error in each byte in the frame
        frame_data[i] ^= 1;

        // Now decode the frame
        ret = yahdlc_get_data(&state, &control, frame_data, frame_length, recv_data,
                              &recv_length);

        // The first and last buffer will return no message. The other data will return invalid FCS
        if ((i == 0) || (i == (frame_length - 1))) {
            TEST_ASSERT_EQUAL_INT(-ENOMSG, ret);
            TEST_ASSERT_EQUAL_INT(0, recv_length);
        } else {
            TEST_ASSERT_EQUAL_INT(-EIO, ret);
            TEST_ASSERT_EQUAL_INT(6, recv_length);
        }
    }
}
