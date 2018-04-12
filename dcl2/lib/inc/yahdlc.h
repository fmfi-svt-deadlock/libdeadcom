/*
The MIT License (MIT)

Original work Copyright (c) 2015 Bang & Olufsen
Modified work Copyright (c) 2018 FMFI ŠVT / Project Deadlock

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------------------------------------------------------------------------

This file was modified for use in Project Deadlock (component libdeadcom). Notable changes:

  - Addition of several new HDLC frames to deal with link establishment and reset
  - Removal of the global state structure and functions dealing with it, since they won't be
    used
  - Change types to represent the correct semantic meaning: if function takes bytes, not a string
    it should use `uint8_t *b`, not `char *b`. If function takes size of buffer as parameter,
    `size_t` should be used, not `unsigned int`.
*/


/**
 * @file yahdlc.h
 */

#ifndef YAHDLC_H
#define YAHDLC_H

#include <stddef.h>
#include <stdint.h>
#include <errno.h>

/** FCS initialization value. */
#define FCS16_INIT_VALUE 0xFFFF

/** FCS value for valid frames. */
#define FCS16_GOOD_VALUE 0xF0B8

/** HDLC start/end flag sequence */
#define YAHDLC_FLAG_SEQUENCE 0x7E

/** HDLC control escape value */
#define YAHDLC_CONTROL_ESCAPE 0x7D

/** HDLC all station address */
#define YAHDLC_ALL_STATION_ADDR 0xFF

/** Supported HDLC frame types */
typedef enum {
    YAHDLC_FRAME_DATA,
    YAHDLC_FRAME_ACK,
    YAHDLC_FRAME_NACK,
    YAHDLC_FRAME_CONN,
    YAHDLC_FRAME_CONN_ACK,
} yahdlc_frame_t;

/** Control field information */
typedef struct {
    yahdlc_frame_t frame;
    uint8_t send_seq_no :3;
    uint8_t recv_seq_no :3;
} yahdlc_control_t;

/**
 * Variables used in yahdlc_get_data and yahdlc_get_data_with_state
 * to keep track of received buffers
 */
typedef struct {
    uint8_t control_escape;
    uint16_t fcs;
    ptrdiff_t start_index;
    ptrdiff_t end_index;
    ptrdiff_t src_index;
    unsigned int frame_byte_index;
    ptrdiff_t dest_index;
    uint8_t frame_control;
} yahdlc_state_t;

/**
 * Calculates a new FCS based on the current value and value of data.
 *
 * @param fcs Current FCS value
 * @param value The value to be added
 * @returns Calculated FCS value
 */
uint16_t fcs16(uint16_t fcs, uint8_t value);

/**
 * Resets values used in yahdlc_get_data function to keep track of received buffers
 */
void yahdlc_reset_state(yahdlc_state_t *state);

/**
 * Retrieves data from specified buffer containing the HDLC frame. Frames can be
 * parsed from multiple buffers e.g. when received via UART.
 *
 * @param[inout] state pointer to a structure used to keep the library state
 * @param[out] control Control field structure with frame type and sequence number
 * @param[in] src Source buffer with frame
 * @param[in] src_len Source buffer length
 * @param[out] dest Destination buffer (should be able to contain max frame size)
 * @param[out] dest_len Destination buffer length
 *
 * @retval >=0 Success (size of returned value should be discarded from source buffer)
 * @retval -EINVAL Invalid parameter
 * @retval -ENOMSG Invalid message
 * @retval -EIO Invalid FCS (size of dest_len should be discarded from source buffer)
 *
 */
int yahdlc_get_data(yahdlc_state_t *state, yahdlc_control_t *control, const uint8_t *src,
                    size_t src_len, uint8_t *dest, size_t *dest_len);

/**
 * Creates HDLC frame with specified data buffer.
 *
 * @param[in] control Control field structure with frame type and sequence number
 * @param[in] src Source buffer with data
 * @param[in] src_len Source buffer length
 * @param[out] dest Destination buffer (should be bigger than source buffer). If this buffer
 *                  frame won't be constructed, only frame length will be computed.
 * @param[out] dest_len Destination buffer length
 *
 * @retval 0 Success
 * @retval -EINVAL Invalid parameter
 */
int yahdlc_frame_data(yahdlc_control_t *control, const uint8_t *src, size_t src_len,
                      uint8_t *dest, size_t *dest_len);

#endif
