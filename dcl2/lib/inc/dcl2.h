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


/**
 * @brief Methods for operations on synchronization primitives
 *
 * These functions return boolean values:
 *  - `true` if the operation succeeded
 *  - `false` if the operation failed
 *
 * If this function returns `false`, the calling function from this library must do just basic
 * necessary cleanup and abort itself with retcode DC_FAILURE.
 * Note that one of these methods *may* be called again during the cleanup. Please ensure that
 * they can handle such case.
 * To signal the exact cause of failure (as opposed to binary failed/succeeded) the error can
 * *for example* be stored in some variable of a struct pointed to by either mutex_p or condvar_p.
 */
typedef struct {
    /**
     * @brief Initialize the Mutex object
     */
    bool (*mutexInit)(void *mutex_p);

    // Lock the mutex
    bool (*mutexLock)(void *mutex_p);

    // Unlock the mutex
    bool (*mutexUnlock)(void *mutex_p);

    // Initialize conditional variable object
    bool (*condvarInit)(void *condvar_p);

    // Wait on conditional variable object with timeout
    // set `timed_out` to true when there was a timeout
    bool (*condvarWait)(void *condvar_p, uint32_t milliseconds, bool *timed_out);

    // Signal conditional variable object
    bool (*condvarSignal)(void *condvar_p);
} DeadcomL2ThreadingMethods;


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

    // Scratchpad buffer for data extraction from newly-received frames
    uint8_t scratchpadBuffer[DEADCOM_PAYLOAD_MAX_LEN];

    // Length of extracted data, if any.
    // Type of this should be size_t, but we need it to be singed and I don't feel like using
    // ssize_t since it comes from POSIX, not the C standard. This code must run on devices that
    // never heard of POSIX.
    int16_t extractionBufferSize;

    // Does extractionBuffer contain the whole message or only a part of it?
    bool extractionComplete;

    // State of the underlying yahdlc library
    yahdlc_state_t yahdlc_state;

    // Outgoing frame number
    uint8_t send_number;

    // Last acknowledged frame number by the other side
    uint8_t next_expected_ack;

    // Number of frame we expect to receive next
    uint8_t recv_number;

    // Number of consectutive communication failures
    uint8_t failure_count;

    // Last response received to some packet we have sent
    DeadcomL2LastResponse last_response;

    // Function for transmitting outgoing bytes
    bool (*transmitBytes)(const uint8_t*, size_t, void*);

    // Pointer to mutex for locking this structure
    void *mutex_p;

    // Pointer to conditional variable to wait on
    void *condvar_p;

    // Transmission context
    void *transmission_context_p;

    // Threading methods
    DeadcomL2ThreadingMethods *t;
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
 *                       transmitted. It returns true if the operation succeeded or false if it
 *                       failed. The library function invoking transmitBytes which has failed shall
 *                       do required cleanup and return DC_EXTMETHOD_FAILED.
 *                       Parameters are:
 *                         - const uint8_t* : the byte buffer to transmit
 *                         - size_t         : size of that byte buffer
 *                         - void*          : Transmission context
 *
 * @retval DC_OK  DeadCom Layer 2 object initialized successfully
 * @retval DC_FAILURE  Invalid parameters or external method has failed
 */
DeadcomL2Result dcInit(DeadcomL2 *deadcom, void *mutex_p, void *condvar_p,
                       DeadcomL2ThreadingMethods *t,
                       bool (*transmitBytes)(const uint8_t*, size_t, void*),
                       void *transmitBytesContext);

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
 * @retval DC_NOT_CONNECTED  The other station has not responded to our request for link
 *                            establishment.
 * @retval DC_FAILURE  Invalid parameters, connection attempt already in progress or external
 *                     method has failed.
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
 * @retval DC_OK  The link close operation will always succeed, independently of what the other
 *                station may think.
 * @retval DC_FAILURE  Attempted to disconnect currently connecting link, invalid parameters or
 *                     external method has failed.
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
 * @retval  DC_OK  If the transmission succeeded and the receiving station has acknowledged the
 *                 message
 * @retval  DC_NOT_CONNECTED  If the link is not in the connected state
 * @retval  DC_LINK_RESET  If the tranission has failed / receiving station failed to acknowledge
 *                         the frame and the link has been reset as the result.
 * @retval  DC_FAILURE  Incorrect parameters, message too long or external method has failed.
 */
DeadcomL2Result dcSendMessage(DeadcomL2 *deadcom, const uint8_t *message, size_t message_len);

/**
 * Get the received message.
 *
 * This function returns the received message, if any. When the message is copied this function
 * transmits a message acknowledgment to the sending station. This means that if this function
 * is not called in time the sending station may decide that we are unresponsive and terminate
 * the link.
 *
 * @param[in] deadcom  Instance of an open DeadCom link
 * @param[out] buffer  Buffer the message will be stored in or NULL. It should be big enough to
 *                     accomodate the whole message. To get required buffer size, call this
 *                     function with this param set to NULL.
 * @param[out] msg_len  Number of bytes that were copied to `buffer` (or would have been copied
 *                      to `buffer` if it wasnt NULL). 0 if no message is pending.
 *
 * @note   Since to get the message you may need to call this function twice (first time to get
 *         the required buffer size, second time to actually copy the message), one might expect
 *         "Time of check to time of use" race condition might occur.
 *         This is only partialy true: if this function returns 0 as `msg_len` it means that there
 *         was no message present at the call time and does not guarantee that next call will
 *         copy 0 bytes.
 *         However, by nature of this library, once this function returns a non-zero in `msg_len`
 *         (thereby singnaling that a message is received and is pending to be picked up), no other
 *         message will be accepted until this one is picked up. Pending message can be cleared only
 *         by "picking it up" by calling this function with non-NULL buffer.
 *         Therefore if a message is pending, subsequent calls to dcGetReceivedMsg are guaranteed
 *         to:
 *           - Either copy the whole message with length as returned by previous invocation of this
 *             method, or
 *           - Return DC_LINK_RESET if link became disconnected in the meantime
 *
 * @retval DC_OK  Operation succeeded
 * @retval DC_LINK_RESET  Link is currently disconnected, no messages can be pending
 * @retval DC_FAILURE  if parameters are invalid or external method has failed
 */
DeadcomL2Result dcGetReceivedMsg(DeadcomL2 *deadcom, uint8_t *buffer, size_t *msg_len);

/**
 * Process received data.
 *
 * This function processes all received bytes in the buffer `data` and triggers appropriate
 * responses. The buffer may contain any chunk of received data, it does not have to contain the
 * whole frame. This processing (and acting upon received data) may take some time, so this function
 * is not to be called from an interrupt handler.
 *
 * @param[in] deadcom  Instance of an open DeadCom link
 * @param[in] data  Data that were just received
 * @param[in] len  Number of received bytes
 *
 * @retval DC_OK  Operation succeeded
 * @retval DC_FAILURE  Invalid parameters or external method has failed
 */
DeadcomL2Result dcProcessData(DeadcomL2 *deadcom, const uint8_t *data, size_t len);


#endif
