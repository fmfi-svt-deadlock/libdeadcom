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
*/


/**
 * @file yahdlc.h
 */

#ifndef YAHDLC_H
#define YAHDLC_H

#include <errno.h>

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
  YAHDLC_FRAME_DISCONNECTED,
} yahdlc_frame_t;

/** Control field information */
typedef struct {
  yahdlc_frame_t frame;
  unsigned char seq_no :3;
} yahdlc_control_t;

/**
 * Variables used in yahdlc_get_data and yahdlc_get_data_with_state
 * to keep track of received buffers
 */
typedef struct {
  char control_escape;
  unsigned short fcs;
  int start_index;
  int end_index;
  int src_index;
  int dest_index;
} yahdlc_state_t;


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
 * @retval >=0 Success (size of returned value should be discarded from source buffer)
 * @retval -EINVAL Invalid parameter
 * @retval -ENOMSG Invalid message
 * @retval -EIO Invalid FCS (size of dest_len should be discarded from source buffer)
 *
 */
int yahdlc_get_data(yahdlc_state_t *state, yahdlc_control_t *control, const char *src,
                    unsigned int src_len, char *dest, unsigned int *dest_len);

/**
 * Creates HDLC frame with specified data buffer.
 *
 * @param[in] control Control field structure with frame type and sequence number
 * @param[in] src Source buffer with data
 * @param[in] src_len Source buffer length
 * @param[out] dest Destination buffer (should be bigger than source buffer)
 * @param[out] dest_len Destination buffer length
 * @retval 0 Success
 * @retval -EINVAL Invalid parameter
 */
int yahdlc_frame_data(yahdlc_control_t *control, const char *src,
                      unsigned int src_len, char *dest, unsigned int *dest_len);

#endif
