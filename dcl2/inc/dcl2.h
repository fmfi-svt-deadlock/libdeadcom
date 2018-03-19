/**
 * @file    dcl2.h
 * @brief   DeadCom Layer 2
 *
 * This file defines interface of DeadCom Layer 2. DeadCom L2 is a library that provides reliable
 * datagram communication over unreliable byte-oriented link. Internally it uses protocol inspired
 * by HDLC to establish a symmetric communication channel. See docs of this repostiry for more info
 * on internal design and protocol specification.
 *
 * This library is designed to be used in multithreaded systems. Details of thread safety are
 * mentioned in docs of each relevant function
 */

#ifndef __DEADCOM_H
#define __DEADCOM_H

#include "stdint.h"
#include "stdbool.h"
#include "yahdlc.h"


#define DEADCOM_CONN_TIMEOUT_MS    100
#define DEADCOM_ACK_TIMEOUT_MS     100
#define DEADCOM_PAYLOAD_MAX_LEN    249
#define DEADCOM_MAX_FAILURE_COUNT  3

// Max frame length is 2 for start and end frame flags + 4 for escaped FCS (worst-case) +
// 4 for escaped address and control byte (worst case) + 2*MAX_PAYLOAD for escaped payload
#define DEADCOM_MAX_FRAME_LEN ((DEADCOM_PAYLOAD_MAX_LEN*2)+10)

typedef enum {
    DC_DISCONNECTED,
    DC_CONNECTING,
    DC_CONNECTED,
    DC_TRANSMITTING
} DeadcomL2State;


typedef enum {
    DC_RESP_OK,
    DC_RESP_REJECT,
    DC_RESP_NOLINK
} DeadcomL2LastResponse;


typedef enum {
    DC_OK,
    DC_FAILURE,
    DC_NOT_CONNECTED,
    DC_LINK_RESET
} DeadcomL2Result;

#define DC_E_NOMSG  -1
#define DC_E_FAIL   -2


/**
 * @brief Methods for operations on synchronization primitives
 */
typedef struct {
    // Initialize mutex object
    void (*mutexInit)(void *mutex_p);

    // Lock the mutex
    void (*mutexLock)(void *mutex_p);

    // Unlock the mutex
    void (*mutexUnlock)(void *mutex_p);

    // Initialize conditional variable object
    void (*condvarInit)(void *condvar_p);

    // Wait on conditional variable object with timeout
    bool (*condvarWait)(void *condvar_p, uint32_t milliseconds);

    // Signal conditional variable object
    void (*condvarSignal)(void *condvar_p);
} DeadcomL2ThreadingVMT;


/**
 * @brief Internal communication driver structure.
 *
 * Don't initialize values of this struct yourself, use the provided initialization function.
 * Those values are for internal use by this library. Don't modify them.
 */
typedef struct {
    // State of the communication library.
    DeadcomL2State state;

    // Buffer for extracted data.
    uint8_t extractionBuffer[DEADCOM_PAYLOAD_MAX_LEN];

    // Length of extracted data, if any.
    int16_t extractionBufferSize;

    // Does extractionBuffer contain the whole message or only a part of it?
    bool extractionComplete;

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

    // Last response received to some packet we have sent
    DeadcomL2LastResponse last_response;

    // Function for transmitting outgoing bytes
    void (*transmitBytes)(uint8_t*, uint8_t);

    // Pointer to mutex for locking this structure
    void *mutex_p;

    // Pointer to conditional variable to wait on
    void *condvar_p;

    // Threading methods
    DeadcomL2ThreadingVMT *t;
} DeadcomL2;


// ---- UPPER LAYER API (for use by Application Protocol implementation) ----

/**
 * Initialize an object representing a DeadCom link.
 *
 * This function initializes an Deadcom object. It does not open a connection.
 *
 * @param deadcom  Instance of Deadcom object to be initialized
 * @param mutex_p  Pointer to a Mutex object, uninitialized
 * @param condvar_p  Pointer to a Conditional Variable object, uninitialized
 * @param t  Threading VMT, methods that can operate on mutex and condvar
 * @param transmitBytes  A function this library will call when it wants to transmit some bytes.
 *                       This function should be blocking and return after all bytes were
 *                       transmitted.
 *
 * @return DC_OK  DeadCom Layer 2 object initialized successfully
 *         DC_FAILURE  Invalid parameters
 */
DeadcomL2Result dcInit(DeadcomL2 *deadcom, void *mutex_p, void *condvar_p,
                       DeadcomL2ThreadingVMT *t, void (*transmitBytes)(uint8_t*, uint8_t));

/**
 * Try to establish a connection.
 *
 * This function tries to establish a connection with the other station. It sends a connection
 * request and waits for response from the other station.
 * If response is received in time it transitions the link to connected state.
 * If response is not received in time it leaves the link in disconnected state.
 *
 * This function blocks the calling thread until the link is established or the operation times out.
 *
 * @param[in] deadcom  Instance of DeadCom link (closed or open)
 *
 * @return DC_OK  The link is now in the connected state, or previously has been in an connected
 *                state.
 *         DC_NOT_CONNECTED  The other station has not responded to our request for link
 *                            establishment.
 *         DC_FAILURE  Invalid parameters or connection attempt already in progress
 */
DeadcomL2Result dcConnect(DeadcomL2 *deadcom);

/**
 * Disconnect a connected link.
 *
 * This function disconnects an already connected link. If the link was already in the disconnected
 * state this function is a no-op. If the link is currently connecting, this function fails.
 *
 * @param[in] deadcom  Instance of DeadCom link (open or closed)
 *
 * @return DC_OK  The link close operation will always succeed, independently of what the other
 *                station may think.
 *         DC_FAILURE  Attempted to disconnect currently connecting link or invalid parameters
 */
DeadcomL2Result dcDisconnect(DeadcomL2 *deadcom);

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
 *          DC_FAILURE  Incorrect parameters or message too long
 */
DeadcomL2Result dcSendMessage(DeadcomL2 *deadcom, const uint8_t *message,
                              const uint8_t message_len);

/**
 * Get received message length.
 *
 * This function returns the length of received message, if any.
 *
 * @param[in] deadcom  Instance of an open DeadCom link
 *
 * @return Length of the received message in bytes or
 *         DC_E_NOMSG if no message is waiting or
 *         DC_E_FAIL  if parameters are invalid
 *
 * @note   Since "dcGetReceivedMsg" and "dcGetReceivedMsg" are 2 different functions, one might
 *         expect that "Time of check to time of use" race condition might occur.
 *         This is only partialy true: if this function returns DC_E_NOMSG it means that there was
 *         no message present at the call time.
 *         However, by nature of this library, once dcGetReceivedMsgLen returns a valid number
 *         of bytes (and therefore a message is received and is pending to be picked up), no other
 *         message will be accepted and subsequent calls to dcGetReceivedMsg are guaranteed to:
 *           - Return the received message, with length as returned by this function
 *           - Return DC_E_NOMSG if link was reset in the meantime
 *         In particular, this means that after dcGetReceivedMsgLen returns a number of bytes,
 *         a buffer of that size is guaranteed to be sufficient to copy the received message.
 */
int16_t dcGetReceivedMsgLen(DeadcomL2 *deadcom);

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
 * @return Length of the received message which was copied to the buffer,
 *         DC_E_NOMSG if no data were copied because no message was present
 *         DC_E_FAIL  if parameters are invalid
 */
int16_t dcGetReceivedMsg(DeadcomL2 *deadcom, uint8_t *buffer);


// ---- LOWER LAYER API (for use by physical layer driver) ----

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
void dcFeedData(DeadcomL2 *deadcom, uint8_t *data, uint8_t len);


#endif
