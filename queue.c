#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

#ifndef strlcpy
#define strlcpy(dst, src, sz) snprintf((dst), (sz), "%s", (src))
#endif

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

struct sort_arg {
    bool descend;
};

static int cmp_func(void *priv,
                    const struct list_head *a,
                    const struct list_head *b)
{
    const struct sort_arg *arg = priv;
    const element_t *ea = list_entry(a, element_t, list);
    const element_t *eb = list_entry(b, element_t, list);

    int cmp = strcmp(ea->value, eb->value);
    if (!arg->descend) {
        return cmp;  // normal ascending
    } else {
        // just flip the sign
        return -cmp;
    }
}

typedef int (*list_cmp_func_t)(void *,
                               const struct list_head *,
                               const struct list_head *);

/*
 * Returns a list organized in an intermediate format suited
 * to chaining of merge() calls: null-terminated, no reserved or
 * sentinel head node, "prev" links not maintained.
 */
static struct list_head *merge(void *priv,
                               list_cmp_func_t cmp,
                               struct list_head *a,
                               struct list_head *b)
{
    struct list_head *head = NULL, **tail = &head;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            *tail = a;
            tail = &a->next;
            a = a->next;
            if (!a) {
                *tail = b;
                break;
            }
        } else {
            *tail = b;
            tail = &b->next;
            b = b->next;
            if (!b) {
                *tail = a;
                break;
            }
        }
    }
    return head;
}

/*
 * Combine final list merge with restoration of standard doubly-linked
 * list structure.  This approach duplicates code from merge(), but
 * runs faster than the tidier alternatives of either a separate final
 * prev-link restoration pass, or maintaining the prev links
 * throughout.
 */
static void merge_final(void *priv,
                        list_cmp_func_t cmp,
                        struct list_head *head,
                        struct list_head *a,
                        struct list_head *b)
{
    struct list_head *tail = head;
    uint8_t count = 0;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            tail->next = a;
            a->prev = tail;
            tail = a;
            a = a->next;
            if (!a)
                break;
        } else {
            tail->next = b;
            b->prev = tail;
            tail = b;
            b = b->next;
            if (!b) {
                b = a;
                break;
            }
        }
    }

    /* Finish linking remainder of list b on to tail */
    tail->next = b;
    do {
        /*
         * If the merge is highly unbalanced (e.g. the input is
         * already sorted), this loop may run many iterations.
         * Continue callbacks to the client even though no
         * element comparison is needed, so the client's cmp()
         * routine can invoke cond_resched() periodically.
         */
        if (unlikely(!++count))
            cmp(priv, b, b);
        b->prev = tail;
        tail = b;
        b = b->next;
    } while (b);

    /* And the final links to make a circular doubly-linked list */
    tail->next = head;
    head->prev = tail;
}

/**
 * list_sort - sort a list
 * @priv: private data, opaque to list_sort(), passed to @cmp
 * @head: the list to sort
 * @cmp: the elements comparison function
 *
 * The comparison function @cmp must return > 0 if @a should sort after
 * @b ("@a > @b" if you want an ascending sort), and <= 0 if @a should
 * sort before @b *or* their original order should be preserved.  It is
 * always called with the element that came first in the input in @a,
 * and list_sort is a stable sort, so it is not necessary to distinguish
 * the @a < @b and @a == @b cases.
 *
 * The comparison function must adhere to specific mathematical properties
 * to ensure correct and stable sorting:
 * - Antisymmetry: cmp(@a, @b) must return the opposite sign of
 * cmp(@b, @a).
 * - Transitivity: if cmp(@a, @b) <= 0 and cmp(@b, @c) <= 0, then
 * cmp(@a, @c) <= 0.
 *
 * This is compatible with two styles of @cmp function:
 * - The traditional style which returns <0 / =0 / >0, or
 * - Returning a boolean 0/1.
 * The latter offers a chance to save a few cycles in the comparison
 * (which is used by e.g. plug_ctx_cmp() in block/blk-mq.c).
 *
 * A good way to write a multi-word comparison is::
 *
 *	if (a->high != b->high)
 *		return a->high > b->high;
 *	if (a->middle != b->middle)
 *		return a->middle > b->middle;
 *	return a->low > b->low;
 *
 *
 * This mergesort is as eager as possible while always performing at least
 * 2:1 balanced merges.  Given two pending sublists of size 2^k, they are
 * merged to a size-2^(k+1) list as soon as we have 2^k following elements.
 *
 * Thus, it will avoid cache thrashing as long as 3*2^k elements can
 * fit into the cache.  Not quite as good as a fully-eager bottom-up
 * mergesort, but it does use 0.2*n fewer comparisons, so is faster in
 * the common case that everything fits into L1.
 *
 *
 * The merging is controlled by "count", the number of elements in the
 * pending lists.  This is beautifully simple code, but rather subtle.
 *
 * Each time we increment "count", we set one bit (bit k) and clear
 * bits k-1 .. 0.  Each time this happens (except the very first time
 * for each bit, when count increments to 2^k), we merge two lists of
 * size 2^k into one list of size 2^(k+1).
 *
 * This merge happens exactly when the count reaches an odd multiple of
 * 2^k, which is when we have 2^k elements pending in smaller lists,
 * so it's safe to merge away two lists of size 2^k.
 *
 * After this happens twice, we have created two lists of size 2^(k+1),
 * which will be merged into a list of size 2^(k+2) before we create
 * a third list of size 2^(k+1), so there are never more than two pending.
 *
 * The number of pending lists of size 2^k is determined by the
 * state of bit k of "count" plus two extra pieces of information:
 *
 * - The state of bit k-1 (when k == 0, consider bit -1 always set), and
 * - Whether the higher-order bits are zero or non-zero (i.e.
 *   is count >= 2^(k+1)).
 *
 * There are six states we distinguish.  "x" represents some arbitrary
 * bits, and "y" represents some arbitrary non-zero bits:
 * 0:  00x: 0 pending of size 2^k;           x pending of sizes < 2^k
 * 1:  01x: 0 pending of size 2^k; 2^(k-1) + x pending of sizes < 2^k
 * 2: x10x: 0 pending of size 2^k; 2^k     + x pending of sizes < 2^k
 * 3: x11x: 1 pending of size 2^k; 2^(k-1) + x pending of sizes < 2^k
 * 4: y00x: 1 pending of size 2^k; 2^k     + x pending of sizes < 2^k
 * 5: y01x: 2 pending of size 2^k; 2^(k-1) + x pending of sizes < 2^k
 * (merge and loop back to state 2)
 *
 * We gain lists of size 2^k in the 2->3 and 4->5 transitions (because
 * bit k-1 is set while the more significant bits are non-zero) and
 * merge them away in the 5->2 transition.  Note in particular that just
 * before the 5->2 transition, all lower-order bits are 11 (state 3),
 * so there is one list of each smaller size.
 *
 * When we reach the end of the input, we merge all the pending
 * lists, from smallest to largest.  If you work through cases 2 to
 * 5 above, you can see that the number of elements we merge with a list
 * of size 2^k varies from 2^(k-1) (cases 3 and 5 when x == 0) to
 * 2^(k+1) - 1 (second merge of case 5 when x == 2^(k-1) - 1).
 */
void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
    struct list_head *list = head->next, *pending = NULL;
    size_t count = 0; /* Count of pending */

    if (list == head->prev) /* Zero or one elements */
        return;

    /* Convert to a null-terminated singly-linked list. */
    head->prev->next = NULL;

    /*
     * Data structure invariants:
     * - All lists are singly linked and null-terminated; prev
     *   pointers are not maintained.
     * - pending is a prev-linked "list of lists" of sorted
     *   sublists awaiting further merging.
     * - Each of the sorted sublists is power-of-two in size.
     * - Sublists are sorted by size and age, smallest & newest at front.
     * - There are zero to two sublists of each size.
     * - A pair of pending sublists are merged as soon as the number
     *   of following pending elements equals their size (i.e.
     *   each time count reaches an odd multiple of that size).
     *   That ensures each later final merge will be at worst 2:1.
     * - Each round consists of:
     *   - Merging the two sublists selected by the highest bit
     *     which flips when count is incremented, and
     *   - Adding an element from the input as a size-1 sublist.
     */
    do {
        size_t bits;
        struct list_head **tail = &pending;

        /* Find the least-significant clear bit in count */
        for (bits = count; bits & 1; bits >>= 1)
            tail = &(*tail)->prev;
        /* Do the indicated merge */
        if (likely(bits)) {
            struct list_head *a = *tail, *b = a->prev;

            a = merge(priv, cmp, b, a);
            /* Install the merged result in place of the inputs */
            a->prev = b->prev;
            *tail = a;
        }

        /* Move one element from input list to pending */
        list->prev = pending;
        pending = list;
        list = list->next;
        pending->next = NULL;
        count++;
    } while (list);

    /* End of input; merge together all the pending lists. */
    list = pending;
    pending = pending->prev;
    for (;;) {
        struct list_head *next = pending->prev;

        if (!next)
            break;
        list = merge(priv, cmp, pending, list);
        pending = next;
    }
    /* The final merge, rebuilding prev links */
    merge_final(priv, cmp, head, pending, list);
}

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

    // Build an 'arg' struct with the chosen ordering
    struct sort_arg arg = {
        .descend = descend,
    };

    // Then just call list_sort. It will:
    // 1) break the ring,
    // 2) do a mergesort in place,
    // 3) re-circularize the list.
    list_sort(&arg, head, cmp_func);
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