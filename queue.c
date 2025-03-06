#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

#ifndef strlcpy
#define strlcpy(dst, src, sz) snprintf((dst), (sz), "%s", (src))
#endif

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

    struct list_head *node;
    list_for_each (node, head) {
        element_t *e = list_entry(node, element_t, list);
        bool dup = false;

        struct list_head *runner = node->next;
        while (runner != head) {
            struct list_head *next = runner->next;

            element_t *r = list_entry(runner, element_t, list);
            if (!strcmp(e->value, r->value)) {
                dup = true;
                list_del(&r->list);
                q_release_element(r);
            }
            runner = next;
        }
        if (dup) {
            struct list_head *next = node->next;
            list_del(&e->list);
            q_release_element(e);
            node = next;
        }
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
void q_reverse(struct list_head *head) {}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend) {}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    return 0;
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    return 0;
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    return 0;
}
