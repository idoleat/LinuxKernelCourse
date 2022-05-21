#include <stddef.h>

#define XCHG(p, n) __atomic_exchange_n(p, n, __ATOMIC_SEQ_CST)
#define STORE(p, n) __atomic_store_n(p, n, __ATOMIC_SEQ_CST)
#define LOAD(p) __atomic_load_n(p, __ATOMIC_SEQ_CST)

struct node {
    struct node *next;
};

#define QUEUE_STATIC_INIT(self)                \
    {                                          \
        .head = &self.stub, .tail = &self.stub \
    }
struct mpscq {
    struct node *head, *tail;
    struct node stub;
};

static void mpscq_create(struct mpscq *self)
{
    self->head = self->tail = &self->stub;
    self->stub.next = NULL;
}

static void mpscq_push(struct mpscq *self, struct node *n)
{
    n->next = 0;
    struct node *prev = XCHG(&self->head, n);
    STORE(&prev->next, n);
}

// we do not want ot use CAS here (heavy).
static struct node *mpscq_pop(struct mpscq *self)
{
    struct node *tail = self->tail, *next = LOAD(&tail->next);
    // see if others have popped or if the queue is empty. If yes, then stop.
    if (tail == &self->stub) {
        if (!next)
            return NULL;

        self->tail = next;
        tail = next;
        next = LOAD(&next->next);
    }

    if (next) {
        self->tail = next;
        return tail;
    }

    struct node *head = LOAD(&self->head);
    if (tail != head)
        return NULL;
    // Deal with ABA problem
    mpscq_push(self, &self->stub);
    next = LOAD(&tail->next);
    if (next) {  // if true, push is successful. If not, we leave (consumer
                 // unaccessable). Cause we only have 1 consumer.
        self->tail = next;
        return tail;
    }
    return NULL;
}

#include <assert.h>
#include <pthread.h>

#define P 8
#define N 4000000L

struct result {
    struct node n;
    long value;
};

struct producer {
    pthread_t t;
    struct mpscq *q;
    long s;
};

static void *produce(void *arg)
{
    struct producer *p = arg;
    struct mpscq *q = p->q;
    static struct result r[P * N];
    for (long i = p->s; i < P * N; i += P) {
        r[i].value = i;
        mpscq_push(q, &r[i].n);
    }
    return 0;
}

int main(void)
{
    struct mpscq q;
    mpscq_create(&q);

    struct producer p[P];
    for (int i = 0; i < P; i++) {
        p[i].q = &q;
        p[i].s = i;
        pthread_create(&p[i].t, 0, produce, p + i);
    }

    static char seen[P * N];
    for (long i = 0; i < P * N; i++) {
        struct result *r;
        do {
            r = (struct result *) mpscq_pop(&q);
        } while (!r);
        assert(!seen[r->value]);
        seen[r->value] = 1;
    }

    for (int i = 0; i < P; i++)
        pthread_join(p[i].t, 0);
    return 0;
}
