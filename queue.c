#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

#ifndef strlcpy
#define strlcpy(dst, src, sz) snprintf((dst), (sz), "%s", (src))
#endif

/*  merges two sorted (non-circular) doubly linked lists */
static struct list_head *merge_two_sorted(struct list_head *left,
                                          struct list_head *right,
                                          bool descend)
{
    struct list_head *head = NULL, **ptr = &head, **node, *prev = NULL;

    for (node = NULL; left && right; *node = (*node)->next) {
        const element_t *l = list_entry(left, element_t, list);
        const element_t *r = list_entry(right, element_t, list);
        bool cmp = (descend) ? (strcmp(l->value, r->value) >= 0)
                             : (strcmp(l->value, r->value) <= 0);
        node = (cmp) ? &left : &right;
        *ptr = *node;
        (*node)->prev = prev;
        prev = *node;
        ptr = &(*ptr)->next;
    }
    *ptr = (struct list_head *) ((uintptr_t) left | (uintptr_t) right);
    (*ptr)->prev = prev;
    return head;
}

/* A standard mergesort for a non-circular, doubly linked list */
static struct list_head *merge_sort_dlist(struct list_head *head, bool descend)
{
    if (!head || !head->next)
        return head;
    // offset fast pointer by 1 to find "first middle", work better for when
    // list that only has 2 elements
    struct list_head *slow = head, *fast = head->next;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }

    struct list_head *mid = slow->next;
    slow->next = NULL;
    mid->prev = NULL;

    struct list_head *left = merge_sort_dlist(head, descend);
    struct list_head *right = merge_sort_dlist(mid, descend);

    return merge_two_sorted(left, right, descend);
}

/*  Convert a circular list with sentinel head into a standard doubly linked
 * list */
static struct list_head *break_ring(struct list_head *head)
{
    if (!head)
        return NULL;
    else if (list_empty(head))
        return NULL;

    struct list_head *first = head->next;
    struct list_head *last = head->prev;

    first->prev = NULL;
    last->next = NULL;

    return first;
}

/* Re-circularize a standard doubly linked list under the sentinel head*/
void recirc(struct list_head *head, struct list_head *first)
{
    if (!first) {
        INIT_LIST_HEAD(head);
        return;
    }
    struct list_head *tail = first;
    while (tail->next)
        tail = tail->next;

    head->next = first;
    first->prev = head;
    head->prev = tail;
    tail->next = head;
}

/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *head = malloc(sizeof(struct list_head));
    if (!head)
        return NULL;
    INIT_LIST_HEAD(head);
    return head;
}

/* Free all storage used by queue */
void q_free(struct list_head *head)
{
    if (!head)
        return;

    element_t *entry = NULL, *safe = NULL;
    list_for_each_entry_safe (entry, safe, head, list) {
        q_release_element(entry);
    }
    free(head);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head || !s)
        return false;

    element_t *e = malloc(sizeof(element_t));
    if (!e)
        return false;

    size_t len = strlen(s) + 1;
    e->value = malloc(len);
    if (!e->value) {
        free(e);
        return false;
    }
    strlcpy(e->value, s, len);
    list_add(&e->list, head);

    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head || !s)
        return false;

    element_t *e = malloc(sizeof(element_t));
    if (!e)
        return false;

    size_t len = strlen(s) + 1;
    e->value = malloc(len);
    if (!e->value) {
        free(e);
        return false;
    }
    strlcpy(e->value, s, len);
    list_add_tail(&e->list, head);

    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;

    element_t *e = list_first_entry(head, element_t, list);
    if (sp)
        strlcpy(sp, e->value, bufsize);
    list_del(&e->list);

    return e;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;

    element_t *e = list_last_entry(head, element_t, list);
    if (sp)
        strlcpy(sp, e->value, bufsize);
    list_del(&e->list);

    return e;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;

    int size = 0;
    struct list_head *node;
    list_for_each (node, head)
        size++;
    return size;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;

    struct list_head *slow, *fast;
    slow = fast = head->next;
    while (fast != head && fast->next != head) {
        slow = slow->next;
        fast = fast->next->next;
    }
    element_t *e = list_entry(slow, element_t, list);
    list_del(&e->list);
    q_release_element(e);
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;

    struct list_head *node = head->next;

    while (node != head) {
        element_t *e = list_entry(node, element_t, list);
        bool dup = false;

        struct list_head *safe = node->next;
        // safe find the first non-duplicated value
        while (safe != head) {
            struct list_head *next = safe->next;
            element_t *s = list_entry(safe, element_t, list);
            if (!strcmp(s->value, e->value)) {
                dup = true;
                list_del(&s->list);
                q_release_element(s);
            } else {
                break;
            }
            safe = next;
        }
        if (dup) {
            list_del(&e->list);
            q_release_element(e);
        }
        node = safe;
    }
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    if (!head || list_empty(head))
        return;

    struct list_head *prev = head, *curr = head->next;
    while (curr != head && curr->next != head) {
        struct list_head *next_pair = curr->next->next;
        struct list_head *next = curr->next;

        next->next = curr;
        curr->next = next_pair;
        prev->next = next;

        next->prev = prev;
        curr->prev = next;
        next_pair->prev = curr;

        prev = curr;
        curr = next_pair;
    }
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;

    struct list_head *node = head;
    do {
        struct list_head *tmp = node->next;
        node->next = node->prev;
        node->prev = tmp;
        node = tmp;
    } while (node != head);
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;

    struct list_head *group_prev = head;
    while (true) {
        int count = 1;
        struct list_head *kth = group_prev->next;
        while (kth != head && count < k) {
            kth = kth->next;
            count++;
        }
        if (kth == head)
            break;

        struct list_head *first = group_prev->next;
        struct list_head *group_next = kth->next;

        struct list_head *node = first;
        while (node != group_next) {
            struct list_head *tmp = node->next;
            node->next = node->prev;
            node->prev = tmp;
            node = tmp;
        }
        group_prev->next = kth;
        kth->prev = group_prev;

        first->next = group_next;
        group_next->prev = first;

        group_prev = first;
    }
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;

    // Break the the list into normal doubly-linked list
    struct list_head *first = break_ring(head);

    struct list_head *sorted = merge_sort_dlist(first, descend);
    recirc(head, sorted);
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return q_size(head);

    struct list_head *node = head->prev;
    const element_t *e = list_entry(node, element_t, list);
    const char *min = e->value;
    while (node->prev != head) {
        element_t *p = list_entry(node->prev, element_t, list);
        if (strcmp(p->value, min) > 0) {
            list_del(&p->list);
            q_release_element(p);
        } else {
            min = p->value;
            node = node->prev;
        }
    }
    return q_size(head);
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return q_size(head);

    struct list_head *node = head->prev;
    const element_t *e = list_entry(node, element_t, list);
    const char *max = e->value;
    while (node->prev != head) {
        element_t *p = list_entry(node->prev, element_t, list);
        if (strcmp(p->value, max) < 0) {
            list_del(&p->list);
            q_release_element(p);
        } else {
            max = p->value;
            node = node->prev;
        }
    }
    return q_size(head);
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    if (!head || list_empty(head))
        return 0;
    else if (list_is_singular(head)) {
        queue_contex_t *q = list_first_entry(head, queue_contex_t, chain);
        return q->size;
    }

    struct list_head *first = head->next;
    struct list_head *node = first->next;
    queue_contex_t *f = list_entry(first, queue_contex_t, chain);

    while (node != head) {
        struct list_head *next = node->next;
        queue_contex_t *n = list_entry(node, queue_contex_t, chain);
        if (n->size == 0 || !n->q)
            continue;

        struct list_head *s1 = break_ring(f->q);
        struct list_head *s2 = break_ring(n->q);
        struct list_head *merged = merge_two_sorted(s1, s2, descend);
        recirc(f->q, merged);
        f->size += n->size;
        n->size = 0;
        INIT_LIST_HEAD(n->q);

        node = next;
    }
    return f->size;
}

/* Shuffle queue using Fisher–Yates shuffle algorithm */
void q_shuffle(struct list_head *head)
{
    if (list_empty(head) || list_is_singular(head)) {
        return;
    }

    int len = q_size(head);
    struct list_head *tail = head->prev;

    for (int i = len - 1; i > 0; i--) {
        int r = rand() % (i + 1);

        struct list_head *random_node = head->next;
        for (int j = 0; j < r; j++) {
            random_node = random_node->next;
        }

        if (random_node != tail) {
            list_move(random_node, tail);
        }
        tail = tail->prev;
    }
}