#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */


/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *head =
        (struct list_head *) malloc(sizeof(struct list_head));
    if (head)
        INIT_LIST_HEAD(head);
    return head;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (!l)
        return;
    element_t *entry, *safe;
    list_for_each_entry_safe (entry, safe, l, list)
        q_release_element(entry);
    free(l);
}

/* Create new element */
element_t *q_new_element(char *s)
{
    element_t *new_element = (element_t *) malloc(sizeof(element_t));
    if (new_element) {
        new_element->value = strdup(s);
        if (!new_element->value) {
            free(new_element);
            new_element = NULL;
        }
    }
    return new_element;
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *new = q_new_element(s);
    if (!new)
        return false;
    list_add(&new->list, head);
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *new = q_new_element(s);
    if (!new)
        return false;
    list_add_tail(&new->list, head);
    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *removed_element = list_first_entry(head, element_t, list);
    if (removed_element) {
        if (sp)
            strncpy(sp, removed_element->value, bufsize);
        list_del(&removed_element->list);
    }
    return removed_element;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *removed_element = list_last_entry(head, element_t, list);
    if (removed_element) {
        if (sp)
            strncpy(sp, removed_element->value, bufsize);
        list_del(&removed_element->list);
    }
    return removed_element;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    int count = 0;
    if (head) {
        struct list_head *node;
        list_for_each (node, head)
            count++;
    }
    return count;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    if (!head || list_empty(head))
        return false;
    struct list_head *slow = head->next;
    for (struct list_head *fast = slow;
         fast != head->prev && fast->next != head->prev;
         fast = fast->next->next)
        slow = slow->next;
    list_del(slow);
    q_release_element(container_of(slow, element_t, list));
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    if (!head)
        return false;
    struct list_head *prev = head;
    element_t *entry, *safe;
    list_for_each_entry_safe (entry, safe, head, list) {
        if (&safe->list != head && !strcmp(entry->value, safe->value)) {
            while (&safe->list != head && !strcmp(entry->value, safe->value)) {
                list_del(&entry->list);
                q_release_element(entry);
                entry = safe;
                safe = list_entry(safe->list.next, element_t, list);
            }
            prev->next = entry->list.next;
            list_del(&entry->list);
            q_release_element(entry);
        } else {
            prev = prev->next;
        }
    }
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    if (!head)
        return;
    struct list_head *node;
    list_for_each (node, head) {
        if (node->next != head)
            list_move(node, node->next);
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
