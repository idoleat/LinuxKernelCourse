/* SPSC-Queue */

#pragma once

#include <stdint.h>
#include <string.h>

#ifndef SPSC_QUEUE_SIZE
#define SPSC_QUEUE_SIZE (1024 * 8)
#endif

/* FIXME: Customize your elements here */
typedef uint32_t ELEMENT_TYPE;
static ELEMENT_TYPE SPSC_QUEUE_ELEMENT_ZERO = 0;

#define SPSC_BATCH_SIZE (SPSC_QUEUE_SIZE / 16)
#define SPSC_BATCH_INCREAMENT (SPSC_BATCH_SIZE / 2)
#define SPSC_CONGESTION_PENALTY (1000) /* spin-cycles */

enum {
    SPSC_OP_SUCCESS = 0,
    SPSC_Q_FULL = -1,
    SPSC_Q_EMPTY = -2,
};

typedef union counter {
    volatile uint32_t w;
    volatile const uint32_t r;
} counter_t;

/* Assume cacheline is 64B */
#define __ALIGN __attribute__((aligned(64)))

typedef struct spsc_queue {
    counter_t head; /* Mostly accessed by producer */
    volatile uint32_t batch_head;
    counter_t tail __ALIGN; /* Mostly accessed by consumer */
    volatile uint32_t batch_tail;
    unsigned long batch_history;

#ifdef SPSC_Q_UNDER_TEST
    uint64_t start_c __ALIGN;
    uint64_t stop_c;
#endif

    ELEMENT_TYPE data[SPSC_QUEUE_SIZE] __ALIGN; /* accessed by prod and coms */
} __ALIGN spsc_queue_t;

static inline uint64_t _read_tsc()
{
    uint64_t time;
    uint32_t msw, lsw;
    __asm__ __volatile__(
        "rdtsc\n\t"
        "movl %%edx, %0\n\t"
        "movl %%eax, %1\n\t"
        : "=r"(msw), "=r"(lsw)
        :
        : "%edx", "%eax");
    time = ((uint64_t) msw << 32) | lsw;
    return time;
}

static inline void _wait_ticks(uint64_t ticks)
{
    uint64_t current_time;
    uint64_t time = _read_tsc();
    time += ticks;
    do {
        current_time = _read_tsc();
    } while (current_time < time);
}

static void queue_init(spsc_queue_t *self)
{
    memset(self, 0, sizeof(spsc_queue_t));
    self->batch_history = SPSC_BATCH_SIZE;
}

static inline int dequeue(spsc_queue_t *self, ELEMENT_TYPE *pValue)
{
    unsigned long batch_size = self->batch_history;
    *pValue = SPSC_QUEUE_ELEMENT_ZERO;

    /* try to zero-in on next batch tail */
    if (self->tail.r == self->batch_tail) {
        uint32_t tmp_tail = self->tail.r + SPSC_BATCH_SIZE;
        if (tmp_tail >= SPSC_QUEUE_SIZE) {
            tmp_tail = 0;
            if (self->batch_history < SPSC_BATCH_SIZE) {
                self->batch_history =
                    (SPSC_BATCH_SIZE <
                     (self->batch_history + SPSC_BATCH_INCREAMENT))
                        ? SPSC_BATCH_SIZE
                        : (self->batch_history + SPSC_BATCH_INCREAMENT);
            }
        }

        batch_size = self->batch_history;
        while (!(self->data[tmp_tail])) {
            _wait_ticks(SPSC_CONGESTION_PENALTY);

            batch_size >>= 1;
            if (batch_size == 0)
                return SPSC_Q_EMPTY;

            tmp_tail = self->tail.r + batch_size;
            if (tmp_tail >= SPSC_QUEUE_SIZE)
                tmp_tail = 0;
        }
        self->batch_history = batch_size;

        if (tmp_tail == self->tail.r)
            tmp_tail = (tmp_tail + 1) >= SPSC_QUEUE_SIZE ? 0 : tmp_tail + 1;
        self->batch_tail = tmp_tail;
    }

    /* actually pull-out data-element */
    *pValue = self->data[self->tail.r];
    self->data[self->tail.r] = SPSC_QUEUE_ELEMENT_ZERO;
    self->tail.w++;
    if (self->tail.r >= SPSC_QUEUE_SIZE)
        self->tail.w = 0;

    return SPSC_OP_SUCCESS;
}

static inline int enqueue(spsc_queue_t *self, ELEMENT_TYPE value)
{
    /* try to zero-in on next batch head */
    if (self->head.r == self->batch_head) {
        uint32_t tmp_head = self->head.r + SPSC_BATCH_SIZE;
        if (tmp_head >= SPSC_QUEUE_SIZE)
            tmp_head = 0;

        if (self->data[tmp_head]) {
            /* run spin cycle penality */
            _wait_ticks(SPSC_CONGESTION_PENALTY);
            return SPSC_Q_FULL;
        }
        self->batch_head = tmp_head;
    }

    self->data[self->head.r] = value;
    self->head.w++;
    if (self->head.r >= SPSC_QUEUE_SIZE)
        self->head.w = 0;

    return SPSC_OP_SUCCESS;
}
