#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "nblist.h"

enum {
    F_MARK = 1, /* dead */
    F_FLAG = 2, /* next is about to die */
    F__MASK = 3,
};

static inline struct nblist_node *n_ptr(uintptr_t x)
{
    return (struct nblist_node *) (x & ~F__MASK);
}

/* set flag on *@pp, passing through backlinks as necessary.
 * @p_val is set to a value of (*@pp)->next. *@pp is updated to the node that
 * points to @n, which may be the list head, or NULL if @n is not on the list
 * anymore. returns true if *@pp points to @n and this flagged it, false
 * otherwise.
 */
static bool try_flag(struct nblist_node **pp,
                     uintptr_t p_val,
                     struct nblist_node *n)
{
    struct nblist_node *p = *pp;
    const uintptr_t new_val = (uintptr_t) n | F_FLAG;

    bool got;
    for (;;) {
        if (p_val == new_val) {
            *pp = p;
            return false;
        }

        uintptr_t old_val = (uintptr_t) n;
        got = atomic_compare_exchange_strong(&p->next, &old_val, new_val);
        if (got || old_val == new_val) {
            /* success, or concurrent flagging. */
            *pp = p;
            return got;
        }

        p_val = old_val;

        /* failure due to concurrent marking. follow backlinks. */
        while ((p_val & F_MARK) != 0) {
            p = atomic_load_explicit(&p->backlink, memory_order_relaxed);
            assert(p);
            p_val = atomic_load_explicit(&p->next, memory_order_relaxed);
        }

        /* @p is no longer @n's parent. walk forward until the parent is
         * found, or return NULL.
         */
        assert(n_ptr(p_val));
        while (n_ptr(p_val) != n) {
            p = n_ptr(p_val);
            p_val = atomic_load_explicit(&p->next, memory_order_relaxed);
            if (!n_ptr(p_val)) {
                *pp = NULL;
                return false;
            }
        }
    }

    *pp = p;
    return got;
}

/* complete removal of @prev -> @n, where @nextval == @n->next. */
static inline void rend_the_marked(struct nblist_node *prev,
                                   struct nblist_node *n,
                                   uintptr_t nextval)
{
    assert((nextval & F_MARK) != 0);
    assert((nextval & F_FLAG) == 0);
    uintptr_t prevval = (uintptr_t) n | F_FLAG;
    atomic_compare_exchange_strong_explicit(
        &prev->next, &prevval, nextval & ~F__MASK, memory_order_release,
        memory_order_relaxed);
}

/* complete removal of @n from flagged parent @prev. */
static void clear_flag(struct nblist_node *prev, struct nblist_node *n)
{
    struct nblist_node *old =
        atomic_exchange_explicit(&n->backlink, prev, memory_order_release);
    assert(!old || old == prev);

    /* set mark, load fresh @n->next. */
    uintptr_t nextval = atomic_load_explicit(&n->next, memory_order_relaxed);
    while ((nextval & F_MARK) == 0) {
        while ((nextval & F_FLAG) != 0) {
            clear_flag(n, n_ptr(nextval));
            nextval = atomic_load(&n->next);
        }
        if (atomic_compare_exchange_strong_explicit(
                &n->next, &nextval, nextval | F_MARK, memory_order_release,
                memory_order_relaxed)) {
            nextval |= F_MARK;
        }
    }

    rend_the_marked(prev, n, nextval);
}

bool nblist_push(struct nblist *list,
                 struct nblist_node *top,
                 struct nblist_node *n)
{
    assert(((uintptr_t) n & F__MASK) == 0);
    uintptr_t old = atomic_load_explicit(&list->n.next, memory_order_acquire);
    while ((old & F_FLAG) != 0) {
        clear_flag(&list->n, n_ptr(old));
        old = atomic_load(&list->n.next);
    }
    assert((old & F_MARK) == 0);
    n->next = old;
    n->backlink = NULL;
    return n_ptr(old) == top && atomic_compare_exchange_strong_explicit(
                                    &list->n.next, &old, (uintptr_t) n,
                                    memory_order_release, memory_order_relaxed);
}

struct nblist_node *nblist_pop(struct nblist *list)
{
    struct nblist_node *p = &list->n;  // head
    uintptr_t p_val = atomic_load(&p->next);
    assert((p_val & F_MARK) == 0);
    struct nblist_node *n = n_ptr(p_val);

    /* find the first n: p -> n where ¬p.flag ∧ ¬p.mark, and atomically set
     * p.flag .
     */
    while (n) {
        if ((p_val & F__MASK) != 0) {
            p = n;
            p_val = atomic_load(&p->next);
        } else if (atomic_compare_exchange_strong(&p->next, &p_val, F_MARK)) {
            break;
        }
        n = n_ptr(p_val);
    }
    if (!n)
        return NULL;

    clear_flag(p, n);
    return n;
}

struct nblist_node *nblist_top(struct nblist *list)
{
    return n_ptr(atomic_load_explicit(&list->n.next, memory_order_acquire));
}

bool nblist_del(struct nblist *list, struct nblist_node *target)
{
    /* find p -> n, where n == target. */
    struct nblist_node *p = &list->n, *n = NULL;
    uintptr_t p_val = atomic_load(&p->next), n_val;
    while (n_ptr(p_val)) {
        n = n_ptr(p_val);
        n_val = atomic_load(&n->next);

        if ((n_val & F_MARK) != 0 && (p_val & F_FLAG) != 0) {
            /* complete an in-progress deletion. */
            rend_the_marked(p, n, n_val);
            if (n == target)
                return false;
            p_val = atomic_load(&p->next);
        } else if (n == target) { /* got it */
            break;
        }
        p = n;
        p_val = n_val;
    }
    if (!n_ptr(p_val))
        return false;

    /* flag and delete. */
    bool got = try_flag(&p, p_val, n);
    if (p)
        clear_flag(p, n);
    return got;
}

static struct nblist_node *skip_dead_nodes(struct nblist_iter *it)
{
    while (it->cur) {
        uintptr_t next =
            atomic_load_explicit(&it->cur->next, memory_order_relaxed);
        if ((next & F_MARK) == 0)
            break;
        /* it->prev remains as before. */
        it->cur = n_ptr(next);
    }
    return it->cur;
}

struct nblist_node *nblist_first(struct nblist *list, struct nblist_iter *it)
{
    it->prev = (struct nblist_node *) &list->n;
    it->cur = n_ptr(atomic_load_explicit(&list->n.next, memory_order_acquire));
    return skip_dead_nodes(it);
}

struct nblist_node *nblist_next(struct nblist *list, struct nblist_iter *it)
{
    it->prev = it->cur;
    it->cur =
        n_ptr(atomic_load_explicit(&it->prev->next, memory_order_relaxed));
    return skip_dead_nodes(it);
}

bool nblist_del_at(struct nblist *list, struct nblist_iter *it)
{
    if (!it->cur)
        return false; /* edge case: cursor at end. */
    if (!it->prev)
        return false; /* repeat case: always NULL */

    uintptr_t cur_val =
        atomic_load_explicit(&it->cur->next, memory_order_relaxed);
    if ((cur_val & F_MARK) != 0)
        return false; /* already gone */

    struct nblist_node *p = it->prev;
    uintptr_t p_val = atomic_load_explicit(&p->next, memory_order_acquire);
    bool got = try_flag(&p, p_val, it->cur);
    it->prev = NULL;
    if (p)
        clear_flag(p, it->cur);
    return got;
}
