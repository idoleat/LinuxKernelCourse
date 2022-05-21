/* Test program for SPSC Queue */

#define _GNU_SOURCE

#define SPSC_Q_UNDER_TEST

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

#include "spsc_queue.h"

#define TEST_SIZE 2000000000

#define N_MAX_CORE 8

static spsc_queue_t queues[N_MAX_CORE];

struct init_info {
    uint32_t cpu_id;
    pthread_barrier_t *barrier;
};

struct init_info info[N_MAX_CORE];

#define INIT_ID(p) (info[p].cpu_id)
#define INIT_BAR(p) (info[p].barrier)
#define INIT_PTR(p) (&info[p])
#define INIT_INFO struct init_info

void *consumer(void *arg)
{
    uint32_t cpu_id;
    cpu_set_t cur_mask;
    uint64_t i;
    ELEMENT_TYPE value = 0, old_value = 0;

    INIT_INFO *init = (INIT_INFO *) arg;
    cpu_id = init->cpu_id;
    pthread_barrier_t *barrier = init->barrier;

    /* user needs tune this according to their machine configurations. */
    CPU_ZERO(&cur_mask);
    CPU_SET(cpu_id * 2, &cur_mask);

    printf("consumer %d:  ---%d----\n", cpu_id, 2 * cpu_id);
    if (sched_setaffinity(0, sizeof(cur_mask), &cur_mask) < 0) {
        printf("Error: sched_setaffinity\n");
        return NULL;
    }

    printf("Consumer created...\n");
    pthread_barrier_wait(barrier);

    queues[cpu_id].start_c = _read_tsc();

    for (i = 1; i <= TEST_SIZE; i++) {
        while (dequeue(&queues[cpu_id], &value) != 0)
            ;

        assert((old_value + 1) == value);
        old_value = value;
    }
    queues[cpu_id].stop_c = _read_tsc();

    printf(
        "consumer: %ld cycles/op\n",
        ((queues[cpu_id].stop_c - queues[cpu_id].start_c) / (TEST_SIZE + 1)));

    pthread_barrier_wait(barrier);
    return NULL;
}

void producer(void *arg, uint32_t num)
{
    uint64_t start_p;
    uint64_t stop_p;
    uint64_t i;
    int32_t j;
    cpu_set_t cur_mask;
    INIT_INFO *init = (INIT_INFO *) arg;
    pthread_barrier_t *barrier = init->barrier;
    ELEMENT_TYPE value = 0;

    /* FIXME: tune this according to machine configurations */
    CPU_ZERO(&cur_mask);
    CPU_SET(0, &cur_mask);
    printf("producer %d:  ---%d----\n", 0, 1);
    if (sched_setaffinity(0, sizeof(cur_mask), &cur_mask) < 0) {
        printf("Error: sched_setaffinity\n");
        return;
    }

    pthread_barrier_wait(barrier);

    start_p = _read_tsc();

    for (i = 1; i <= TEST_SIZE + SPSC_BATCH_SIZE; i++) {
        for (j = 1; j < num; j++) {
            value = i;
            while (enqueue(&queues[j], value) != 0)
                ;
        }
    }
    stop_p = _read_tsc();

    printf("producer %ld cycles/op\n",
           (stop_p - start_p) / ((TEST_SIZE + 1) * (num - 1)));

    pthread_barrier_wait(barrier);
}

int main(int argc, char *argv[])
{
    pthread_t consumer_thread;
    pthread_attr_t consumer_attr;
    pthread_barrier_t barrier;

    int max_th = 2;
    if (argc > 1)
        max_th = atoi(argv[1]);

    if (max_th < 2) {
        max_th = 2;
        printf("Minimum core number is 2\n");
    }
    if (max_th > N_MAX_CORE) {
        max_th = N_MAX_CORE;
        printf("Maximum core number is %d\n", max_th);
    }

    srand((unsigned int) _read_tsc());

    for (int i = 0; i < N_MAX_CORE; i++)
        queue_init(&queues[i]);

    int error = pthread_barrier_init(&barrier, NULL, max_th);
    if (error != 0) {
        perror("BW");
        return 1;
    }
    error = pthread_attr_init(&consumer_attr);
    if (error != 0) {
        perror("BW");
        return 1;
    }

    /* For N cores, there are N-1 fifos. */
    for (int i = 1; i < max_th; i++) {
        INIT_ID(i) = i;
        INIT_BAR(i) = &barrier;
        error = pthread_create(&consumer_thread, &consumer_attr, consumer,
                               INIT_PTR(i));
    }
    if (error != 0) {
        perror("BW");
        return 1;
    }

    INIT_ID(0) = 0;
    INIT_BAR(0) = &barrier;
    producer(INIT_PTR(0), max_th);
    printf("Done!\n");

    return 0;
}
