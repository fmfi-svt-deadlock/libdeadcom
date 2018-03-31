/**
 * @file    leaky-pipe.h
 * @brief   Thread-safe byte-oriented link emulator with configurable deterministic unreliability.
 *
 * This file defines a public API for the Leaky Pipe library.
 */

#ifndef __LEAKY_PIPE_H
#define __LEAKY_PIPE_H

#include <stdint.h>
#include <pthread.h>
#include "pipe.h"


/**
 * @brief Corruption definition for a single byte
 */
typedef struct {
    /** Something bad happens with byte on this position **/
    unsigned int position;

    /** This value influences what happens to that byte **/
    uint8_t value;
} lp_corrupt_def_t;


/**
 * @brief Definition of pipe errors
 *
 * This structure defines what transmission errors will the leaky pipe cause.
 *
 * TODO lists and probabilities and what and when.
 */
typedef struct {
    /**
     * Seed to the pseudo-random bad-luck generator.
     */
    unsigned int seed;

    /**
     * A list of byte positions that should be dropped. This list must contain values sorted from
     * lowest to highest.
     *
     * Example: If this list contains values {0, 3, 4}, then  the first, fourth and fifth
     * transmitted bytes will never arrive at the destination. Tragic.
     */
    unsigned int *drop_list;
    /**
     * Length of the `drop_list` array
     */
    unsigned int drop_list_len;

    /**
     * A list of errors to be introduced to the transmission. Structs in this array must be sorted
     * by member `.position` from lowest to highest. If position matches the byte that is currently
     * being transmitted, it will be XORed with `.value`.
     */
    lp_corrupt_def_t *corrupt_list;
    /**
     * Length of the `corrupt_list` array
     */
    unsigned int corrupt_list_len;

    /**
     * A list of bytes to be added to the transmission. Structs in this array must be sorted by
     * member `.position` from lowest to highest. If byte with number `.position` can be transmitted
     * next, `.value` will be transmitted first. If `.position` is 0 `.value` is transmitted
     * immediatelly after leaky pipe initialization. If `.position` is 1, `.value` will be
     * transmitted immediatelly after first byte.
     *
     * These bytes *do not* increase the counter of sent bytes, and therefore do not affect
     * `drop_list` and `corrupt_list`.
     *
     * Example:
     *  - drop_list = {2, 3}
     *  - corrupt_list = {(0, 20), (3, 20)}
     *  - add_list = {(0, 0xF0), (0, 0x0D), (4, 0xFF)}
     *
     *  - bytes to be transmitted: {0x10, 0x15, 0x20, 0x25, 0x30}
     *  - result: {0xF0 (a), 0x0D (a), 0x04 (c), 0x15 (ok), (d), (d), 0xFF (a), 0x30 (ok)}
     */
    lp_corrupt_def_t *add_list;
    /**
     * Length of the `add_list` array
     */
    unsigned int add_list_len;

    /**
     * Probability of dropping each packet.
     */
    float drop_prob;

    /**
     * Probability of corrupting each packet with single-bit error.
     */
    float corrupt_prob;

    /**
     * Probability of adding a packet. Opportunity to add opens after each transmitted packet
     * (including added transmitted packets!). That means that if this value is `1` the pipe will
     * send noise endlessly after first transmitted packet, and lp_transmit function will never
     * return. Take care!.
     */
    float add_prob;
} lp_args_t;

/**
 * @brief A structure representing a leaky pipe.
 *
 * Internals of this structure should be touched only by this library.
 */
typedef struct {
    pipe_producer_t *pipe_producer;
    pipe_consumer_t *pipe_consumer;
    pthread_mutex_t mutex;
    unsigned int random_state;
    long byte_counter;
    unsigned int drop_list_cntr;
    unsigned int corrupt_list_cntr;
    unsigned int add_list_cntr;
    lp_args_t holes;
} leaky_pipe_t;


/**
 * @brief Initializes structure of leaky pipe parameters
 *
 * This function initializes `lp_args_t` structure. After initialization the structure contains
 * values to create a flawless pipe or can be modified to add faults to the new pipe.
 *
 * @param[out] args   A structure to be initialized
 *
 * @note Seed will be initialized to 1. You may want to specify a different value.
 */
void lp_init_args(lp_args_t *args);


/**
 * @brief Creates a new leaky pipe
 *
 * @param[out] lp   Newly created leaky pipe
 * @param[in] args  A structure specifying pipe faults
 */
void lp_init(leaky_pipe_t *lp, lp_args_t *args);


/**
 * @brief Free resources associated with the leaky pipe
 *
 * @param[in] lp  Leaky pipe to be decommissioned
 */
void lp_free(leaky_pipe_t *lp);


/**
 * @brief Sends bytes to the pipe
 *
 * This function corrupts and enqueues corrupted data to this pipe. If the pipe is full, this
 * function will block the calling thread until more space is available.
 *
 * @param[in] lp   Leaky pipe
 *
 * This function is thread-safe.
 */
void lp_transmit(leaky_pipe_t *lp, uint8_t byte);


/**
 * @brief Receives bytes from the pipe
 *
 * This function will get bytes from the pipe. If there are no bytes waiting, it will block until
 * some appear.
 *
 * @param[in]  lp           Leaky pipe
 * @param[out] buffer       Destination buffer for bytes from this pipe
 * @param[in]  buffer_size  Size of the destination buffer
 *
 * @return Number of bytes received from the pipe.
 */
unsigned int lp_receive(leaky_pipe_t *lp, uint8_t *buffer, unsigned int buffer_size);


/**
 * @brief Cut-off the pipe
 *
 * This function closes the pipe. After this function returns, any calls to lp_transmit are still
 * permitted, but any transmitted data will be discarded. Any calls to lp_receive will stop
 * blocking and after all bytes are read, will repeatedly return 0.
 *
 * @param[in]  lp  Leaky pipe
 */
void lp_cutoff(leaky_pipe_t *lp);

#endif
