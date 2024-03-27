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
void q_reverse(struct list_head *head)
{
    if (!head)
        return;
    struct list_head *node, *safe;
    list_for_each_safe (node, safe, head) {
        list_move(node, head);
    }
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    if (!head)
        return;
    struct list_head *node = head->next;
    while (node != head) {
        struct list_head *group_next = node, *group_prev = node->prev;
        int count = 0;
        while (count < k && group_next != head) {
            count++;
            group_next = group_next->next;
        }
        if (count < k)
            break;
        while (node != group_next) {
            struct list_head *safe = node->next;
            list_move(node, group_prev);
            node = safe;
        }
    }
}
/* merge two sorted queue */
void q_merge_two_list(struct list_head *l_head,
                      struct list_head *r_head,
                      bool descend)
{
    LIST_HEAD(head);
    element_t *l_entry, *l_safe, *r_entry, *r_safe;
    for (l_entry = list_entry((l_head)->next, __typeof__(*l_entry), list),
        l_safe = list_entry(l_entry->list.next, __typeof__(*l_entry), list),
        r_entry = list_entry((r_head)->next, __typeof__(*r_entry), list),
        r_safe = list_entry(r_entry->list.next, __typeof__(*r_entry), list);
         &l_entry->list != (l_head) && &r_entry->list != (r_head);) {
        if ((strcmp(l_entry->value, r_entry->value) < 0 && !descend) ||
            (strcmp(l_entry->value, r_entry->value) > 0 && descend)) {
            list_move_tail(&l_entry->list, &head);
            l_entry = l_safe;
            l_safe = list_entry(l_safe->list.next, __typeof__(*l_entry), list);
        } else {
            list_move_tail(&r_entry->list, &head);
            r_entry = r_safe;
            r_safe = list_entry(r_safe->list.next, __typeof__(*r_entry), list);
        }
    }

    if (&l_entry->list != l_head) {
        list_splice(l_head, r_head);
    }
    list_splice(&head, r_head);
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;
    struct list_head *slow = head->next;
    for (struct list_head *fast = slow;
         fast != head->prev && fast->next != head->prev;
         fast = fast->next->next)
        slow = slow->next;
    LIST_HEAD(l_head);
    list_cut_position(&l_head, head, slow);
    q_sort(head, descend);
    q_sort(&l_head, descend);
    q_merge_two_list(&l_head, head, descend);
}


/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;
    q_reverse(head);
    element_t *prev = NULL, *entry, *safe;
    list_for_each_entry_safe (entry, safe, head, list) {
        if (prev && strcmp(prev->value, entry->value) <= 0) {
            list_del(&entry->list);
            q_release_element(entry);
        }
        prev = list_entry(safe->list.prev, element_t, list);
    }
    q_reverse(head);
    return q_size(head);
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;
    q_reverse(head);
    element_t *prev = NULL, *entry, *safe;
    list_for_each_entry_safe (entry, safe, head, list) {
        if (prev && strcmp(prev->value, entry->value) >= 0) {
            list_del(&entry->list);
            q_release_element(entry);
        }
        prev = list_entry(safe->list.prev, element_t, list);
    }
    q_reverse(head);
    return q_size(head);
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    return 0;
}
