/* non-blocking singly-linked lists.
 *
 * the algorithm used was presented by Fomitchev and Ruppert in "Lock-Free
 * Linked Lists and Skip Lists" (2003).
 *
 * this implementation supports insert at the list head ("push"), deletion at
 * any point, and iteration. the intrusive link structure ("nblist_node") must
 * be aligned to 4; due to storage of metadata in the low bits, structures
 * pointed to solely with this mechanism might not show up as reachable in
 * valgrind etc.
 */

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* container_of() - Calculate address of object that contains address ptr
 * @ptr: pointer to member variable
 * @type: type of the structure containing ptr
 * @member: name of the member variable in struct @type
 *
 * Return: @type pointer of object containing ptr
 */
#ifndef container_of
#define container_of(ptr, type, member)                            \
    __extension__({                                                \
        const __typeof__(((type *) 0)->member) *__pmember = (ptr); \
        (type *) ((char *) __pmember - offsetof(type, member));    \
    })
#endif

struct nblist_node {
    _Atomic uintptr_t next;
    struct nblist_node *_Atomic backlink;
} __attribute__((aligned(4)));

struct nblist {
    struct nblist_node n;
};

#define NBSL_LIST_INIT(name)            \
    {                                   \
        {                               \
            .next = 0, .backlink = NULL \
        }                               \
    }

static inline void nblist_init(struct nblist *list)
{
    struct nblist proto = NBSL_LIST_INIT(proto);
    *list = proto;
}

/* push @n at the head of @list, if the previous head == @top. returns true if
 * successful, false if the caller should refetch @top and try again.
 */
bool nblist_push(struct nblist *list, struct nblist_node *top, struct nblist_node *n);

/* pop first node from @list, returning it or NULL. */
struct nblist_node *nblist_pop(struct nblist *list);

/* peek first node in @list, returning it or NULL. */
struct nblist_node *nblist_top(struct nblist *list);

/* remove @n from @list. O(n).
 *
 * returns true if the current thread removed @n from @list; false if some
 * other thread did it, or if removal was deferred due to concurrent access of
 * the previous node. in the first case @n will have gone away once nblist_del()
 * returns; in the second, @n will have been removed from @list once every
 * concurrent call to nblist_del() and nblist_push() have returned.
 */
bool nblist_del(struct nblist *list, struct nblist_node *n);

struct nblist_iter {
    struct nblist_node *prev, *cur;
};

/* iteration with option to delete. this is always read-only, i.e. never
 * causes writes to any node along the chain. it skips over dead nodes, but
 * the ones it returns may appear dead nonetheless due to concurrent delete.
 */
struct nblist_node *nblist_first(struct nblist *list, struct nblist_iter *it);
struct nblist_node *nblist_next(struct nblist *list, struct nblist_iter *it);

/* attempt to remove value returned from previous call to nblist_{first,next}(),
 * returning true on success and false on failure. @it remains robust against
 * concurrent mutation; subsequent calls to nblist_del_at() before nblist_next()
 * always return false.
 *
 * a sequence of nblist_del_at() and nblist_next() can be used to pop all nodes
 * from @list from a certain point onward.
 */
bool nblist_del_at(struct nblist *list, struct nblist_iter *it);