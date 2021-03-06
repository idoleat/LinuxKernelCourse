#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "queue.h"

/*
 * Create empty queue.
 * Return NULL if could not allocate space.
 */
queue_t *q_new()
{
    queue_t *q = malloc(sizeof(queue_t));
    if (q != NULL) {
        q->head = NULL;
        q->tail = NULL;
        q->size = 0;
    }
    return q;
}

/* Free all storage used by queue */
void q_free(queue_t *q)
{
    /* TODO: How about freeing the list elements and the strings? */
    /* => itterate through all the elements and free them */
    if (q == NULL)
        return;

    list_ele_t *it = q->head;
    while (it != NULL) {
        list_ele_t *next = it->next;
        free(it->value);
        free(it);
        it = next;
    }
    /* Free queue structure */
    free(q);
}

/*
 * Attempt to insert element at head of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(queue_t *q, char *s)
{
    if (q == NULL)
        return false;

    list_ele_t *newh;
    /* TODO: What should you do if the q is NULL? */
    newh = malloc(sizeof(list_ele_t));
    if (newh == NULL)
        return false;

    char *d = malloc(strlen(s) + 1);
    if (d == NULL) {
        free(newh);
        return false;
    }
    strncpy(d, s, strlen(s) + 1);
    /* Don't forget to allocate space for the string and copy it */
    /* What if either call to malloc returns NULL? */
    newh->value = d;
    newh->next = q->head;
    q->size++;
    if (q->size == 1)
        q->tail = newh;
    q->head = newh;
    return true;
}

/*
 * Attempt to insert element at tail of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(queue_t *q, char *s)
{
    if (q == NULL)
        return false;

    list_ele_t *newt;
    /* TODO: What should you do if the q is NULL? */
    newt = malloc(sizeof(list_ele_t));
    if (newt == NULL)
        return false;

    char *d = malloc(strlen(s) + 1);
    if (d == NULL) {
        free(newt);
        return false;
    }
    strncpy(d, s, strlen(s) + 1);

    newt->value = d;
    newt->next = NULL;
    q->size++;
    if (q->size == 1)
        q->head = newt;
    else
        q->tail->next = newt;
    q->tail = newt;
    return true;
}

/*
 * Attempt to remove element from head of queue.
 * Return true if successful.
 * Return false if queue is NULL or empty.
 * If sp is non-NULL and an element is removed, copy the removed string to *sp
 * (up to a maximum of bufsize-1 characters, plus a null terminator.)
 * The space used by the list element and the string should be freed.
 */
bool q_remove_head(queue_t *q, char *sp, size_t bufsize)
{
    if (q == NULL)
        return false;
    if (q->head == NULL)
        return false;

    if (sp != NULL) {
        strncpy(sp, q->head->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }

    list_ele_t *tmp = q->head;
    q->head = q->head->next;
    q->size--;
    free(tmp->value);
    free(tmp);
    return true;
}

/*
 * Return number of elements in queue.
 * Return 0 if q is NULL or empty
 */
int q_size(queue_t *q)
{
    if (q == NULL)
        return 0;

    return q->size;
}

/*
 * Reverse elements in queue
 * No effect if q is NULL or empty
 * This function should not allocate or free any list elements
 * (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
 * It should rearrange the existing ones.
 */
void q_reverse(queue_t *q)
{
    if (q == NULL)
        return;
    if (q->head == NULL)
        return;

    list_ele_t *prev = NULL, *current = q->head, *look_ahead;

    q->tail = q->head;
    while (current != NULL) {
        look_ahead = current->next;
        current->next = prev;
        prev = current;
        current = look_ahead;
    }
    q->head = prev;
}

/*
 * Sort elements of queue in ascending order
 * No effect if q is NULL or empty. In addition, if q has only one
 * element, do nothing.
 */
void q_sort(queue_t *q)
{
    if (q == NULL)
        return;
    if (q->head == NULL || q->size == 1)
        return;

    q->head = merge_sort(q->head);
    while (q->tail->next) {
        q->tail = q->tail->next;
    }
}

list_ele_t *merge_sort(list_ele_t *head)
{
    if (head == NULL || head->next == NULL)
        return head;

    list_ele_t *left = head, *right = head->next;  // try `right = head` later
    while (right && right->next) {
        left = left->next;
        right = right->next->next;
    }
    right = left->next;
    left->next = NULL;

    return merge(merge_sort(head), merge_sort(right));
}

list_ele_t *merge(list_ele_t *left, list_ele_t *right)
{
    list_ele_t *head = NULL, *currrent = NULL;

    // attach head
    size_t LL = strlen(left->value);
    size_t RR = strlen(right->value);
    if (strncmp(left->value, right->value, LL > RR ? LL : RR) < 0) {
        currrent = left;
        left = left->next;
    } else {
        currrent = right;
        right = right->next;
    }
    head = currrent;

    while (left != NULL && right != NULL) {
        size_t L = strlen(left->value);
        size_t R = strlen(right->value);
        if (strncmp(left->value, right->value, L > R ? L : R) < 0) {
            currrent->next = left;
            left = left->next;
        } else {
            currrent->next = right;
            right = right->next;
        }
        currrent = currrent->next;
    }

    if (left != NULL)
        currrent->next = left;
    else
        currrent->next = right;

    return head;
}