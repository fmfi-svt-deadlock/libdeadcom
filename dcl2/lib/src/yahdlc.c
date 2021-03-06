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
  - I-frames now send N(S) and N(R) (as opposed to only N(S)) as specified by HDLC
  - Fix frame decoding problems if control-octet transparency is applied to address byte or
    control byte
  - Calling yahdlc_frame_data with NULL destination buffer will now compute the length of required
    buffer
  - Fix valgrind-revealed uninitialized vars
  - Return valid control struct from yahdlc_get_data if and only if return value is >= 0
    (and therefore valid frame was received), so that caller doesn't have to keep track of control
    struct from previous calls when parsing byte-by-byte from serial link.
  - Change types to represent the correct semantic meaning: if function takes 8-bit bytes, not a
    string of characters, it should use `uint8_t *b`, not `char *b`. If function takes size of
    buffer as parameter, `size_t` should be used, not `unsigned int`. `unsigned short` is not
    guaranteed to be 16 bits, use `uint16_t` instead.
*/

#include "yahdlc.h"

// HDLC Control field bit positions
#define YAHDLC_CONTROL_S_OR_U_FRAME_BIT 0
#define YAHDLC_CONTROL_SEND_SEQ_NO_BIT 1
#define YAHDLC_CONTROL_S_FRAME_TYPE_BIT 2
#define YAHDLC_CONTROL_POLL_BIT 4
#define YAHDLC_CONTROL_RECV_SEQ_NO_BIT 5
#define YAHDLC_CONTROL_U_ONLY_FRAME_BIT 1

// HDLC Control type definitions
#define YAHDLC_CONTROL_TYPE_RECEIVE_READY 0
#define YAHDLC_CONTROL_TYPE_RECEIVE_NOT_READY 1
#define YAHDLC_CONTROL_TYPE_REJECT 2
#define YAHDLC_CONTROL_TYPE_SELECTIVE_REJECT 3

// HDLC U-type mask definitions
#define YAHDLC_CONTROL_CONN_OR_DISC 2
#define YAHDLC_CONTROL_CONN_ACK 5
#define YAHDLC_CONTROL_U_TYPE_ADDED_BITS 3
#define YAHDLC_CONTROL_CONN_SPECIAL_BIT 5

#define YAHDLC_U_FRAME_CONN_CHECK 0x01
#define YAHDLC_U_FRAME_CONN_ACK_CHECK 0x03


static const uint16_t fcstab[256] = { 0x0000, 0x1189, 0x2312, 0x329b,
    0x4624, 0x57ad, 0x6536, 0x74bf, 0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c,
    0xdbe5, 0xe97e, 0xf8f7, 0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c,
    0x75b7, 0x643e, 0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff,
    0xe876, 0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5, 0x3183,
    0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 0xbdcb, 0xac42,
    0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974, 0x4204, 0x538d, 0x6116,
    0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb, 0xce4c, 0xdfc5, 0xed5e, 0xfcd7,
    0x8868, 0x99e1, 0xab7a, 0xbaf3, 0x5285, 0x430c, 0x7197, 0x601e, 0x14a1,
    0x0528, 0x37b3, 0x263a, 0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960,
    0xbbfb, 0xaa72, 0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630,
    0x17b9, 0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 0xffcf,
    0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70, 0x8408, 0x9581,
    0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 0x0840, 0x19c9, 0x2b52,
    0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff, 0x9489, 0x8500, 0xb79b, 0xa612,
    0xd2ad, 0xc324, 0xf1bf, 0xe036, 0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5,
    0x4f6c, 0x7df7, 0x6c7e, 0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7,
    0xc03c, 0xd1b5, 0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74,
    0x5dfd, 0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c, 0xc60c,
    0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 0x4a44, 0x5bcd,
    0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb, 0xd68d, 0xc704, 0xf59f,
    0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232, 0x5ac5, 0x4b4c, 0x79d7, 0x685e,
    0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a,
    0xb0a3, 0x8238, 0x93b1, 0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb,
    0x0e70, 0x1ff9, 0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9,
    0x8330, 0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

uint16_t fcs16(uint16_t fcs, uint8_t value) {
    return (fcs >> 8) ^ fcstab[(fcs ^ value) & 0xff];
}


void yahdlc_escape_value(uint8_t value, uint8_t *dest, ptrdiff_t *dest_index) {
    // Check and escape the value if needed
    if ((value == YAHDLC_FLAG_SEQUENCE) || (value == YAHDLC_CONTROL_ESCAPE)) {
        if (dest) {
            dest[(*dest_index)++] = YAHDLC_CONTROL_ESCAPE;
            value ^= 0x20;
        } else {
            (*dest_index)++;
        }
    }

    // Add the value to the destination buffer and increment destination index
    if (dest) {
        dest[(*dest_index)++] = value;
    } else {
        (*dest_index)++;
    }
}


yahdlc_control_t yahdlc_get_control_type(uint8_t control) {
    yahdlc_control_t value = {};

    // Check if the frame is a S-frame (or U-frame)
    if (control & (1 << YAHDLC_CONTROL_S_OR_U_FRAME_BIT)) {
        // Check if only U-frame type
        if (control & (1 << YAHDLC_CONTROL_U_ONLY_FRAME_BIT)) {
            if ((control >> YAHDLC_CONTROL_CONN_SPECIAL_BIT) == YAHDLC_U_FRAME_CONN_CHECK) {
                value.frame = YAHDLC_FRAME_CONN;
            } else if ((control >> YAHDLC_CONTROL_CONN_SPECIAL_BIT) == YAHDLC_U_FRAME_CONN_ACK_CHECK) {
                value.frame = YAHDLC_FRAME_CONN_ACK;
            }
        } else {
            // Check if S-frame type is a Receive Ready (ACK)
            if (((control >> YAHDLC_CONTROL_S_FRAME_TYPE_BIT) & 0x3)
            == YAHDLC_CONTROL_TYPE_RECEIVE_READY) {
                value.frame = YAHDLC_FRAME_ACK;
            } else {
                // Assume it is an NACK since Receive Not Ready, Selective Reject and U-frames are
                // not supported
                value.frame = YAHDLC_FRAME_NACK;
            }
            // Add the receive sequence number from the S-frame
            value.recv_seq_no = (control >> YAHDLC_CONTROL_RECV_SEQ_NO_BIT) & 0x7;
        }
    } else {
        // It must be an I-frame so add the send sequence number
        value.frame = YAHDLC_FRAME_DATA;
        value.send_seq_no = (control >> YAHDLC_CONTROL_SEND_SEQ_NO_BIT) & 0x7;
        value.recv_seq_no = (control >> YAHDLC_CONTROL_RECV_SEQ_NO_BIT) & 0x7;
    }

    return value;
}


uint8_t yahdlc_frame_control_type(yahdlc_control_t *control) {
    uint8_t value = 0;

    // For details see: https://en.wikipedia.org/wiki/High-Level_Data_Link_Control
    switch (control->frame) {
        case YAHDLC_FRAME_DATA:
            // Create the HDLC I-frame control byte with Poll bit set
            value |= (control->send_seq_no << YAHDLC_CONTROL_SEND_SEQ_NO_BIT);
            value |= (control->recv_seq_no << YAHDLC_CONTROL_RECV_SEQ_NO_BIT);
            value |= (1 << YAHDLC_CONTROL_POLL_BIT);
            break;

        case YAHDLC_FRAME_ACK:
            // Create the HDLC Receive Ready S-frame control byte with Poll bit cleared
            value |= (control->recv_seq_no << YAHDLC_CONTROL_RECV_SEQ_NO_BIT);
            value |= (1 << YAHDLC_CONTROL_S_OR_U_FRAME_BIT);
            break;

        case YAHDLC_FRAME_NACK:
            // Create the HDLC Receive Ready S-frame control byte with Poll bit cleared
            value |= (control->recv_seq_no << YAHDLC_CONTROL_RECV_SEQ_NO_BIT);
            value |= (YAHDLC_CONTROL_TYPE_REJECT << YAHDLC_CONTROL_S_FRAME_TYPE_BIT);
            value |= (1 << YAHDLC_CONTROL_S_OR_U_FRAME_BIT);
            break;

        case YAHDLC_FRAME_CONN:
            // Create the HDLC SABM U-frame control byte with Poll bit set
            value |= (1 << YAHDLC_CONTROL_S_OR_U_FRAME_BIT);
            value |= (1 << YAHDLC_CONTROL_U_ONLY_FRAME_BIT);
            value |= (1 << YAHDLC_CONTROL_POLL_BIT);
            value |= (YAHDLC_CONTROL_U_TYPE_ADDED_BITS << YAHDLC_CONTROL_CONN_OR_DISC);
            value |= (1 << YAHDLC_CONTROL_CONN_SPECIAL_BIT);
            break;

        case YAHDLC_FRAME_CONN_ACK:
            // Create the HDLC UA U-frame control byte with Poll bit cleared
            value |= (1 << YAHDLC_CONTROL_S_OR_U_FRAME_BIT);
            value |= (1 << YAHDLC_CONTROL_U_ONLY_FRAME_BIT);
            value |= (YAHDLC_CONTROL_U_TYPE_ADDED_BITS << YAHDLC_CONTROL_CONN_ACK);
            break;
    }
    return value;
}


void yahdlc_reset_state(yahdlc_state_t *state, size_t max_frame_len) {
    state->fcs = FCS16_INIT_VALUE;
    state->start_index = state->end_index = -1;
    state->src_index = state->dest_index = 0;
    state->control_escape = 0;
    state->frame_byte_index = 0;
    state->frame_control = 0;
    state->max_frame_len = max_frame_len;
}


int yahdlc_get_data(yahdlc_state_t *state, yahdlc_control_t *control, const uint8_t *src,
                    size_t src_len, uint8_t *dest, size_t *dest_len) {
    int ret;
    uint8_t value;
    size_t i;

    // Make sure that all parameters are valid
    if (!state || !control || !src || !dest || !dest_len) {
        return -EINVAL;
    }

    // Run through the data bytes
    for (i = 0; i < src_len; i++) {
        // First find the start flag sequence
        if (state->start_index < 0) {
            if (src[i] == YAHDLC_FLAG_SEQUENCE) {
                // Check if an additional flag sequence byte is present
                if ((i < (src_len - 1)) && (src[i + 1] == YAHDLC_FLAG_SEQUENCE)) {
                    // Just loop again to silently discard it (accordingly to HDLC)
                    continue;
                }

                state->start_index = state->src_index;
            }
        } else {
            // Check for end flag sequence
            if (src[i] == YAHDLC_FLAG_SEQUENCE) {
                // Check if an additional flag sequence byte is present or earlier received
                if (((i < (src_len - 1)) && (src[i + 1] == YAHDLC_FLAG_SEQUENCE))
                || ((state->start_index + 1) == state->src_index)) {
                    // Just loop again to silently discard it (accordingly to HDLC)
                    continue;
                }

                state->end_index = state->src_index;
                break;
            } else if (src[i] == YAHDLC_CONTROL_ESCAPE) {
                state->control_escape = 1;
            } else {
                // Update the value based on any control escape received
                if (state->control_escape) {
                    state->control_escape = 0;
                    value = src[i] ^ 0x20;
                } else {
                    value = src[i];
                }

                // Now update the FCS value
                state->fcs = fcs16(state->fcs, value);

                if (state->frame_byte_index == 1) {
                    // Control field is the second byte after the start flag sequence
                    state->frame_control = value;
                } else if (state->frame_byte_index > 1) {
                    // Start adding the data values after the Control field to the buffer
                    if ((size_t)state->dest_index+1 >= state->max_frame_len) {
                        // Maximum frame length hit. Discard what we've seen so far from the buffer
                        // and reset internal state. Parsing will commence mid-frame, which will
                        // be discarded since no start flag will be found
                        *dest_len = i;
                        ret = -EIO;
                        yahdlc_reset_state(state, state->max_frame_len);
                        return ret;
                    }
                    dest[state->dest_index++] = value;
                }
                state->frame_byte_index++;
            }
        }
        state->src_index++;
    }

    // Check for invalid frame (no start or end flag sequence)
    if ((state->start_index < 0) || (state->end_index < 0)) {
        // Return no message and make sure destination length is 0
        *dest_len = 0;
        ret = -ENOMSG;
    } else {
        // A frame is at least 4 bytes in size and has a valid FCS value
        if ((state->end_index < (state->start_index + 4))
        || (state->fcs != FCS16_GOOD_VALUE)) {
            // Return FCS error and indicate that data up to end flag sequence in buffer should
            // be discarded
            *dest_len = i;
            ret = -EIO;
        } else {
            // Good frame. Decode control byte
            *control = yahdlc_get_control_type(state->frame_control);
            // Return success and indicate that data up to end flag sequence in buffer should be
            // discarded
            *dest_len = state->dest_index - sizeof(state->fcs);
            ret = i;
        }

        // Reset values for next frame
        yahdlc_reset_state(state, state->max_frame_len);
    }

    return ret;
}

int yahdlc_frame_data(yahdlc_control_t *control, const uint8_t *src,
                      size_t src_len, uint8_t *dest, size_t *dest_len) {
    size_t i;
    ptrdiff_t dest_index = 0;
    uint8_t value = 0;
    uint16_t fcs = FCS16_INIT_VALUE;

    // Make sure that all parameters are valid
    if (!control || (!src && (src_len > 0)) || !dest_len) {
        return -EINVAL;
    }

    // Start by adding the start flag sequence
    if (dest) {
        dest[dest_index++] = YAHDLC_FLAG_SEQUENCE;
    } else {
        dest_index++;
    }

    // Add the all-station address from HDLC (broadcast)
    fcs = fcs16(fcs, YAHDLC_ALL_STATION_ADDR);
    yahdlc_escape_value(YAHDLC_ALL_STATION_ADDR, dest, &dest_index);

    // Add the framed control field value
    value = yahdlc_frame_control_type(control);
    fcs = fcs16(fcs, value);
    yahdlc_escape_value(value, dest, &dest_index);

    // Only DATA frames should contain data
    if (control->frame == YAHDLC_FRAME_DATA) {
        // Calculate FCS and escape data
        for (i = 0; i < src_len; i++) {
            fcs = fcs16(fcs, src[i]);
            yahdlc_escape_value(src[i], dest, &dest_index);
        }
    }

    // Invert the FCS value accordingly to the specification
    fcs ^= 0xFFFF;

    // Run through the FCS bytes and escape the values
    for (i = 0; i < sizeof(fcs); i++) {
        value = ((fcs >> (8 * i)) & 0xFF);
        yahdlc_escape_value(value, dest, &dest_index);
    }

    // Add end flag sequence and update length of frame
    if (dest) {
        dest[dest_index++] = YAHDLC_FLAG_SEQUENCE;
    } else {
        dest_index++;
    }
    *dest_len = dest_index;

    return 0;
}
