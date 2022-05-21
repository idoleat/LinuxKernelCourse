#define MPMC_IMPLEMENTATION
#include "mpmc.h"

#if defined(__STDC_NO_THREADS__)
#error "Your C compiler does not support C11 threads."
#endif

#include <stdio.h>
#include <threads.h>

#define MAX_CONCURRENT 4
#define MAX_TEST_LOOP_COUNT 10000

#define LOG(x)      \
    printf(x "\n"); \
    fflush(stdout);
#define TEST_SIMPLE(x)    \
    if (!(x)) {           \
        LOG("FAIL: " #x); \
        return 1;         \
    }

int test_null()
{
    TEST_SIMPLE(mpmc_init(NULL) == Queue_Error_Null);

    return 0;
}

int test_not_aligned()
{
    struct {
        Mpmc_State mpmc[2];
    } state;

    intptr_t pmpmc = 1 | (intptr_t) state.mpmc;

    Mpmc_State *bad_mpmc = (Mpmc_State *) pmpmc;

    TEST_SIMPLE(mpmc_init(bad_mpmc) == Queue_Error_Not_Aligned_16_Bytes);

    return 0;
}

int test_init_ok()
{
    _Alignas(16) struct {
        Mpmc_State mpmc;
    } state;

    TEST_SIMPLE(mpmc_init(&state.mpmc) == Queue_Ok);

    return 0;
}

int test_enqueue()
{
    _Alignas(16) struct {
        Mpmc_State mpmc;
    } state;

    struct {
        Mpmc_Type mpmc;
    } data = {
        {0},
    };

    TEST_SIMPLE(mpmc_init(&state.mpmc) == Queue_Ok);

    TEST_SIMPLE(mpmc_try_enqueue(&state.mpmc, &data.mpmc) == Queue_Ok);

    return 0;
}

int test_dequeue()
{
    _Alignas(16) struct {
        Mpmc_State mpmc;
    } state;

    struct {
        Mpmc_Type mpmc;
    } data = {
        {1},
    };

    struct {
        Mpmc_Type mpmc;
    } result = {
        {0},
    };

    TEST_SIMPLE(mpmc_init(&state.mpmc) == Queue_Ok);

    TEST_SIMPLE(mpmc_try_enqueue(&state.mpmc, &data.mpmc) == Queue_Ok);

    TEST_SIMPLE(mpmc_try_dequeue(&state.mpmc, &result.mpmc) == Queue_Ok);

    TEST_SIMPLE(data.mpmc.payload == result.mpmc.payload);

    return 0;
}

int test_empty()
{
    _Alignas(16) struct {
        Mpmc_State mpmc;
    } state;

    struct {
        Mpmc_Type mpmc;
    } result = {
        {0},
    };

    TEST_SIMPLE(mpmc_init(&state.mpmc) == Queue_Ok);

    TEST_SIMPLE(mpmc_try_dequeue(&state.mpmc, &result.mpmc) == Queue_Empty);

    return 0;
}

int test_full()
{
    _Alignas(16) struct {
        Mpmc_State mpmc;
    } state;

    TEST_SIMPLE(mpmc_init(&state.mpmc) == Queue_Ok);

    for (uint64_t i = 0; i < MPMC_ITEM_COUNT; i++) {
        Mpmc_Type data = {i};
        TEST_SIMPLE(mpmc_try_enqueue(&state.mpmc, &data) == Queue_Ok);
    }

    struct {
        Mpmc_Type mpmc;
    } data = {{MPMC_ITEM_COUNT}};

    TEST_SIMPLE(mpmc_try_enqueue(&state.mpmc, &data.mpmc) == Queue_Full);

    return 0;
}

typedef enum Queue_Type { Queue_Type_Mpmc } Queue_Type;

typedef struct Queue_State {
    Queue_Type type;

    union {
        Mpmc_State *mpmc;
    };
} Queue_State;

typedef struct Queue_Data {
    atomic_size_t *start;
    Queue_State state;
    size_t thread_i_count;
    size_t thread_o_count;
    size_t count_in;
    size_t count_out;
} Queue_Data;

int enqueue(void *raw)
{
    Queue_Data *qd = (Queue_Data *) raw;

    while (!atomic_load_explicit(qd->start, memory_order_relaxed))
        ;

    size_t thread_id = (size_t) thrd_current();
    size_t count = qd->count_in;
    Queue_State state = qd->state;

    switch (state.type) {
    case Queue_Type_Mpmc: {
        for (size_t i = 0; i < count; i++) {
            Mpmc_Type data = {thread_id + i};
            while (mpmc_try_enqueue(state.mpmc, &data) != Queue_Ok)
                ;
        }
        break;
    }
    }

    return 0;
}

int dequeue(void *raw)
{
    Queue_Data *qd = (Queue_Data *) raw;

    while (!atomic_load_explicit(qd->start, memory_order_relaxed))
        ;

    size_t count = qd->count_out;
    Queue_State state = qd->state;

    switch (state.type) {
    case Queue_Type_Mpmc: {
        for (size_t i = 0; i < count; i++) {
            Mpmc_Type data = {0};
            while (mpmc_try_dequeue(state.mpmc, &data) != Queue_Ok)
                ;
        }
        break;
    }
    }

    return 0;
}

int test_queue(Queue_State state, size_t thread_i_count, size_t thread_o_count)
{
    thrd_t threads_i[MAX_CONCURRENT] = {0};
    thrd_t threads_o[MAX_CONCURRENT] = {0};

    atomic_size_t start;
    atomic_store(&start, 0);

    Queue_Data queue_data = {&start,
                             state,
                             thread_i_count,
                             thread_o_count,
                             MAX_TEST_LOOP_COUNT * thread_o_count,
                             MAX_TEST_LOOP_COUNT * thread_i_count};

    for (size_t i = 0; i < queue_data.thread_i_count; i++) {
        TEST_SIMPLE(thrd_create(&threads_i[i], enqueue, &queue_data) ==
                    thrd_success);
    }

    for (size_t i = 0; i < queue_data.thread_o_count; i++) {
        TEST_SIMPLE(thrd_create(&threads_o[i], dequeue, &queue_data) ==
                    thrd_success);
    }

    // go
    atomic_store_explicit(&start, 1, memory_order_relaxed);

    // finish
    for (size_t i = 0; i < queue_data.thread_i_count; i++) {
        int result_in = 0;

        TEST_SIMPLE(thrd_join(threads_i[i], &result_in) == thrd_success);

        TEST_SIMPLE(result_in == 0);
    }

    for (size_t i = 0; i < queue_data.thread_o_count; i++) {
        int result_out = 0;

        TEST_SIMPLE(thrd_join(threads_o[i], &result_out) == thrd_success);

        TEST_SIMPLE(result_out == 0);
    }

    return 0;
}

int test_queue_x_in_x_out(size_t thread_i_count, size_t thread_o_count)
{
    _Alignas(16) Mpmc_State state_mpmc;

    TEST_SIMPLE(mpmc_init(&state_mpmc) == Queue_Ok);

    Queue_State queue_states[4] = {{Queue_Type_Mpmc, {.mpmc = &state_mpmc}}};

    int result = 0;

    result += test_queue(queue_states[0], thread_i_count, thread_o_count);

    if (thread_i_count == 1) {
        result += test_queue(queue_states[1], thread_i_count, thread_o_count);
    }

    if (thread_o_count == 1) {
        result += test_queue(queue_states[2], thread_i_count, thread_o_count);
    }

    if ((thread_i_count == 1) && (thread_o_count == 1)) {
        result += test_queue(queue_states[3], thread_i_count, thread_o_count);
    }

    return result;
}

int main(int argument_count, char **arguments)
{
    (void) argument_count;
    (void) arguments;

#define RUN_SUITE(x) \
    LOG("TEST: " #x) \
    if (x) {         \
        return 1;    \
    }

    RUN_SUITE(test_null());
    RUN_SUITE(test_not_aligned());
    RUN_SUITE(test_init_ok());
    RUN_SUITE(test_enqueue());
    RUN_SUITE(test_dequeue());
    RUN_SUITE(test_empty());
    RUN_SUITE(test_full());

    return 0;
}