/* basic interface tests on the non-blocking singly-linked lists. */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "nblist.h"

struct item {
    struct nblist_node link;
    int value;
};

static struct item *push(struct nblist *list, int value)
{
    struct item *it = malloc(sizeof(*it));
    it->value = value;
    int spins = 0;
    while (!nblist_push(list, nblist_top(list), &it->link)) {
        /* spin */
        printf("%s: repeating", __func__);
        if (++spins == 10)
            abort();
    }
    return it;
}

static struct item *pop(struct nblist *list)
{
    struct nblist_node *link = nblist_pop(list);
    return container_of(link, struct item, link);
}

static struct item *top(struct nblist *list)
{
    struct nblist_node *link = nblist_top(list);
    return container_of(link, struct item, link);
}

int main(void)
{
    struct nblist *list = malloc(sizeof(*list));
    nblist_init(list);
    struct item *n1 = push(list, 1);
    push(list, 2);
    push(list, 3);
    push(list, 4);

    /* test "pop" while items exist. */
    assert(top(list));
    assert(pop(list)->value == 4);
    assert(top(list));
    assert(top(list)->value == 3);

    /* test nblist_del() from top. */
    assert(nblist_del(list, nblist_top(list)));
    assert(top(list));
    assert(top(list)->value == 2);

    /* test nblist_del() from bottom. */
    assert(nblist_del(list, &n1->link));
    assert(top(list));
    assert(top(list)->value == 2);

    /* popping the last element */
    assert(pop(list)->value == 2);
    assert(!top(list));
    assert(!pop(list));

    return 0;
}