/**
 * @file    deadcom.h
 * @brief   Deadlock Communicator Library
 *
 * This file defines interface of DeadCom. DeadCom is a library that powers communication
 * between various Deadlock components. Internally it uses protocol inspired by HDLC to establish
 * a symmetric communication channel. See docs of this repostiry for more infor on internal design
 * and protocol specification.
 *
 * This library is designed to be used in multithreaded systems. Details of thread safety are
 * mentioned in docs of each relevant function
 */

#ifndef __DEADCOM_H
#define __DEADCOM_H

#include "stdint.h"
#include "yahdlc.h"

typedef enum {
    DC_DISCONNECTED,
    DC_CONNECTING,
    DC_CONNECTED
} DeadcomState;


typedef enum {
    DC_OK,
    DC_FAILURE,
    DC_NOT_CONNECTED,
    DC_LINK_RESET
} DeadcomResult;


/**
 * @brief Internal communication driver structure.
 *
 * Don't initialize values of this struct yourself, use the provided initialization function.
 * Those values are for internal use by this library. Don't modify them.
 */
typedef struct {
    // State of the communication library.
    DeadcomState state;

    // Buffer for incoming messages. Specification guarantees that the message can't be longer
    // than 256 bytes.
    uint8_t messageBuffer[256];

    // Buffer for extracted data.
    uint8_t extractionBuffer[256];

    // State of the underlying yahdlc library
    yahdlc_state_t yahdlc_state;

    // Outgoing frame number
    uint8_t send_number;

    // Last acknowledged frame number
    uint8_t last_acked;

    // Incoming frame number
    uint8_t recv_number;

    // Number of consectutive communication failures
    uint8_t failure_count;

    // Function for transmitting outgoing bytes
    void (*transmitBytes)(uint8_t*, uint8_t);
} Deadcom;

// ---- UPPER LAYER API ----

/**
 * Initialize an object representing a DeadCom link.
 *
 * This function initializes an Deadcom object. It does not open a connection.
 *
 * @param deadcom  Instance of Deadcom object to be initialized
 * @param transmitBytes  A function this library will call when it wants to transmit some bytes.
 *                       This function should be blocking and return after all bytes were
 *                       transmitted.
 */
void dcInit(Deadcom *deadcom, void (*transmitBytes)(uint8_t*, uint8_t));

/**
 * Try to establish a connection.
 *
 * This function tries to establish a connection with the other station. If the link is already
 * connected this function is a no-op.
 *
 * @param[in] deadcom  Instance of DeadCom link (closed or open)
 *
 * @return DC_OK  The link is now in the connected state, or previously has been in an connected
 *                state.
 *         DC_FAILURE  The other station has not responded to our request for link establishment.
 */
DeadcomResult dcConnect(Deadcom *deadcom);

/**
 * Disconnect a connected link.
 *
 * This function disconnects an already connected link. If the link was already in the disconnected
 * state this function is a no-op.
 *
 * @param[in] deadcom  Instance of DeadCom link (open or closed)
 *
 * @return DC_OK  The link close operation will always succeed, independently of what the other
 *                station may think.
 */
DeadcomResult dcDisconnect(Deadcom *deadcom);

/**
 * Transmit a message.
 *
 * This function encapsulates the given message and transmits it over open link. It then waits
 * for confirmation that the message has been received by the receiving side. It automatically
 * tries to retransmit the message several times if the confirmation didn't arrive.
 * It returns after the message has been acknowledged by the receiving side or transmission failed
 * and the link was reset.
 *
 * If a message is currently being transmitted over this link, this function waits for that
 * transmission and acknowledgment reception to complete.
 *
 * @param[in] deadcom  Instance of an open DeadCom link
 * @param[in] message  Message to be transmitted
 * @param[in] message_len  Length of the message to be transmitted
 *
 * @returns DC_OK  If the transmission succeeded and the receiving station has acknowledged the
 *                 message
 *          DC_NOT_CONNECTED  If the link is not in the connected state
 *          DC_LINK_RESET  If the tranission has failed / receiving station failed to acknowledge
 *                         the frame and the link has been reset as the result.
 */
DeadcomResult dcSendMessage(Deadcom *deadcom, uint8_t *message, uint8_t message_len);

/**
 * Get received message length.
 *
 * This function returns the length of received message, if any.
 *
 * @param[in] deadcom  Instance of an open DeadCom link
 *
 * @return Length of the received message in bytes, 0 if no received message is present in the
 *         buffer (because it has not been received yet, or the link is down, …).
 */
uint8_t dcGetReceivedMsgLen(Deadcom *deadcom);

/**
 * Get the received message.
 *
 * This function returns the received message, in any. When the message is copied this function
 * transmits a message acknowledgment to the sending station. This means that if this function
 * is not called in time the sending station may decide that we are unresponsive and terminate
 * the link.
 *
 * @param[in] deadcom  Instance of an open DeadCom link
 * @param[out] message  Buffer the message will be stored in. It should be big enough to accomodate
 *                      the whole message. Use `dcGetReceivedMsgLen` to get length of the message
 *                      in the buffer.
 *
 * @return Length of the received message which was copied to the buffer, or 0 if no message
 *         was present in the buffer.
 */
uint8_t dcGetReceivedMsg(Deadcom *deadcom, uint8_t *buffer);

// ---- LOWER LAYER API ----

/**
 * Pass received data to the library.
 *
 * This will cause the library to try to parse all received data. This call may take some time, so
 * don't call it from interrupt handler.
 *
 * @param[in] deadcom  Instance of an open DeadCom link
 * @param[in] data  Data that were just received
 * @param[in] len  Number of received bytes
 */
void dcFeedData(Deadcom *deadcom, uint8_t *data, uint8_t len);


#endif