#ifndef VIBE_SYS_LIST_H
#define VIBE_SYS_LIST_H

/**
 * @file sys/list.h
 * @brief Intrusive doubly-linked list for VibeRTOS internal data structures.
 *
 * Nodes are embedded directly in the owning struct and accessed via
 * CONTAINER_OF. This avoids dynamic allocation for list membership.
 *
 * Example usage:
 *   struct my_item {
 *       int value;
 *       vibe_list_node_t node;
 *   };
 *
 *   vibe_list_t my_list;
 *   vibe_list_init(&my_list);
 *
 *   struct my_item item = { .value = 42 };
 *   vibe_list_append(&my_list, &item.node);
 *
 *   VIBE_LIST_FOR_EACH(&my_list, n) {
 *       struct my_item *it = CONTAINER_OF(n, struct my_item, node);
 *       // use it->value
 *   }
 */

#include <stddef.h>
#include <stdbool.h>
#include "vibe/sys/util.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Types
 * --------------------------------------------------------------------- */

/** A node embedded in a struct to make it list-able. */
typedef struct vibe_list_node {
    struct vibe_list_node *prev;
    struct vibe_list_node *next;
} vibe_list_node_t;

/** The list head/anchor. Does not own any data itself. */
typedef struct vibe_list {
    vibe_list_node_t *head;   /**< First node, or NULL if empty. */
    vibe_list_node_t *tail;   /**< Last node, or NULL if empty. */
    uint32_t          count;  /**< Number of nodes currently in the list. */
} vibe_list_t;

/* -----------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a list to empty state.
 */
static inline void vibe_list_init(vibe_list_t *list)
{
    list->head  = NULL;
    list->tail  = NULL;
    list->count = 0U;
}

/**
 * @brief Initialise a lone node (not attached to any list).
 */
static inline void vibe_list_node_init(vibe_list_node_t *node)
{
    node->prev = NULL;
    node->next = NULL;
}

/* -----------------------------------------------------------------------
 * Queries
 * --------------------------------------------------------------------- */

/** @return true if the list has no nodes. */
static inline bool vibe_list_is_empty(const vibe_list_t *list)
{
    return list->head == NULL;
}

/** @return Number of nodes in the list. */
static inline uint32_t vibe_list_count(const vibe_list_t *list)
{
    return list->count;
}

/** @return Pointer to the first node, or NULL. */
static inline vibe_list_node_t *vibe_list_peek_head(const vibe_list_t *list)
{
    return list->head;
}

/** @return Pointer to the last node, or NULL. */
static inline vibe_list_node_t *vibe_list_peek_tail(const vibe_list_t *list)
{
    return list->tail;
}

/* -----------------------------------------------------------------------
 * Insertion
 * --------------------------------------------------------------------- */

/**
 * @brief Append node to the end of the list (O(1)).
 */
static inline void vibe_list_append(vibe_list_t *list, vibe_list_node_t *node)
{
    node->next = NULL;
    node->prev = list->tail;

    if (list->tail != NULL) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->count++;
}

/**
 * @brief Prepend node to the front of the list (O(1)).
 */
static inline void vibe_list_prepend(vibe_list_t *list, vibe_list_node_t *node)
{
    node->prev = NULL;
    node->next = list->head;

    if (list->head != NULL) {
        list->head->prev = node;
    } else {
        list->tail = node;
    }
    list->head = node;
    list->count++;
}

/**
 * @brief Insert node after an existing node already in the list.
 */
static inline void vibe_list_insert_after(vibe_list_t      *list,
                                           vibe_list_node_t *after,
                                           vibe_list_node_t *node)
{
    node->prev = after;
    node->next = after->next;

    if (after->next != NULL) {
        after->next->prev = node;
    } else {
        list->tail = node;
    }
    after->next = node;
    list->count++;
}

/* -----------------------------------------------------------------------
 * Removal
 * --------------------------------------------------------------------- */

/**
 * @brief Remove a node from the list (O(1), node must be in the list).
 */
static inline void vibe_list_remove(vibe_list_t *list, vibe_list_node_t *node)
{
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    node->prev = NULL;
    node->next = NULL;
    list->count--;
}

/**
 * @brief Remove and return the head node, or NULL if empty.
 */
static inline vibe_list_node_t *vibe_list_get_head(vibe_list_t *list)
{
    vibe_list_node_t *node = list->head;
    if (node != NULL) {
        vibe_list_remove(list, node);
    }
    return node;
}

/* -----------------------------------------------------------------------
 * Iteration macros
 * --------------------------------------------------------------------- */

/**
 * VIBE_LIST_FOR_EACH — iterate all nodes in list order.
 *
 * @param list  Pointer to vibe_list_t.
 * @param node  Loop variable of type vibe_list_node_t *.
 *
 * Safe against read-only traversal. NOT safe if nodes are removed during
 * iteration — use VIBE_LIST_FOR_EACH_SAFE for that.
 */
#define VIBE_LIST_FOR_EACH(list, node) \
    for ((node) = (list)->head; (node) != NULL; (node) = (node)->next)

/**
 * VIBE_LIST_FOR_EACH_SAFE — iterate, safe against removal of current node.
 *
 * @param list  Pointer to vibe_list_t.
 * @param node  Loop variable of type vibe_list_node_t *.
 * @param tmp   Temporary variable of the same type (declared by caller).
 */
#define VIBE_LIST_FOR_EACH_SAFE(list, node, tmp)                   \
    for ((node) = (list)->head,                                    \
         (tmp)  = ((node) != NULL ? (node)->next : NULL);          \
         (node) != NULL;                                           \
         (node) = (tmp),                                           \
         (tmp)  = ((node) != NULL ? (node)->next : NULL))

#ifdef __cplusplus
}
#endif

#endif /* VIBE_SYS_LIST_H */
