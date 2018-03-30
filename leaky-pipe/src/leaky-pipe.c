#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include "pipe.h"
#include "leaky-pipe.h"

static const lp_args_t good_pipe = {
    .seed = 1,

    .drop_list = NULL,
    .drop_list_len = 0,

    .corrupt_list = NULL,
    .corrupt_list_len = 0,

    .add_list = NULL,
    .add_list_len = 0,

    .drop_prob = 0,
    .corrupt_prob = 0,
    .add_prob = 0
};


void lp_init_args(lp_args_t *args) {
    memcpy(args, &good_pipe, sizeof(lp_args_t));
}


void lp_init(leaky_pipe_t *lp, lp_args_t *args) {
    pipe_t *p = pipe_new(sizeof(uint8_t), 0);
    lp->pipe_producer = pipe_producer_new(p);
    lp->pipe_consumer = pipe_consumer_new(p);
    pipe_free(p);

    lp->random_state = args->seed;
    lp->byte_counter = 0;
    lp->drop_list_cntr = 0;
    lp->corrupt_list_cntr = 0;
    lp->add_list_cntr = 0;
    memcpy(&(lp->holes), args, sizeof(lp_args_t));

    pthread_mutex_init(&(lp->mutex), NULL);

    while (lp->add_list_cntr < lp->holes.add_list_len &&
           lp->holes.add_list[lp->add_list_cntr].position == 0) {
        const uint8_t b[] = {lp->holes.add_list[lp->add_list_cntr].value};
        pipe_push(lp->pipe_producer, b, 1);
        lp->add_list_cntr++;
    }
}


void lp_transmit(leaky_pipe_t *lp, uint8_t byte) {
    pthread_mutex_lock(&(lp->mutex));
    bool drop = false;

    lp->byte_counter++;

    while (lp->corrupt_list_cntr < lp->holes.corrupt_list_len &&
           lp->holes.corrupt_list[lp->corrupt_list_cntr].position == lp->byte_counter-1) {
        byte ^= lp->holes.corrupt_list[lp->corrupt_list_cntr].value;
        lp->corrupt_list_cntr++;
    }

    while (lp->drop_list_cntr < lp->holes.drop_list_len &&
           lp->holes.drop_list[lp->drop_list_cntr] == lp->byte_counter-1) {
        drop = true;
        lp->drop_list_cntr++;
    }

    if (((float)rand_r(&(lp->random_state)) / (float)RAND_MAX) <= lp->holes.corrupt_prob) {
        byte ^= (1 << (rand_r(&(lp->random_state)) % 8));
    }

    if (((float)rand_r(&(lp->random_state)) / (float)RAND_MAX) <= lp->holes.drop_prob) {
        drop = true;
    }

    if (!drop) {
        const uint8_t b[] = {byte};
        pipe_push(lp->pipe_producer, b, 1);
    }

    while (lp->add_list_cntr < lp->holes.add_list_len &&
           lp->holes.add_list[lp->add_list_cntr].position == lp->byte_counter) {
        const uint8_t b[] = {lp->holes.add_list[lp->add_list_cntr].value};
        pipe_push(lp->pipe_producer, b, 1);
        lp->add_list_cntr++;
    }

    while (((float)rand_r(&(lp->random_state)) / (float)RAND_MAX) <= lp->holes.add_prob) {
        const uint8_t b[] = {(uint8_t)(rand_r(&(lp->random_state)) % 256)};
        pipe_push(lp->pipe_producer, b, 1);
    }

    pthread_mutex_unlock(&(lp->mutex));
}


unsigned int lp_receive(leaky_pipe_t *lp, uint8_t *buffer, unsigned int buffer_size) {
    return pipe_pop(lp->pipe_consumer, buffer, buffer_size);
}
