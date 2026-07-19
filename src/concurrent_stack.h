#ifndef STACK_H
#define STACK_H

#include <stdlib.h>
#include <stdatomic.h>

#endif

#ifndef STACK_NAME
#error "STACK_NAME must be defined"
#endif

#ifndef STACK_TYPE
#error "STACK_TYPE must be defined"
#endif

#ifndef STACK_MALLOC
#define STACK_MALLOC(size) malloc(size)
#define STACK_MALLOC_DEFINED
#endif

#ifndef STACK_FREE
#define STACK_FREE(ptr) free(ptr)
#define STACK_FREE_DEFINED
#endif

#define STACK_CONCAT_(a, b) a ## b
#define STACK_CONCAT(a, b) STACK_CONCAT_(a, b)
#define STACK_FUNC(name) STACK_CONCAT(STACK_NAME, _##name)
#define STACK_TYPED(name) STACK_CONCAT(STACK_NAME, _##name)

#define STACK_NODE STACK_TYPED(node)
#define STACK_HEAD STACK_TYPED(head)

typedef struct STACK_NODE {
    struct STACK_NODE *next;
    STACK_TYPE value;
} STACK_NODE;

// Need double-wide compare-and-swap (DWCAS) for atomic stack
typedef struct STACK_HEAD {
    size_t version;
    STACK_NODE *node;
} STACK_HEAD;

#define STACK_ITEM_MEMORY_POOL_NAME STACK_TYPED(node_memory_pool)

#define MEMORY_POOL_NAME STACK_ITEM_MEMORY_POOL_NAME
#define MEMORY_POOL_TYPE STACK_NODE
#include "concurrent_memory_pool/concurrent_memory_pool.h"
#undef MEMORY_POOL_NAME
#undef MEMORY_POOL_TYPE

#define STACK_ITEM_MEMORY_POOL_FUNC(name) STACK_CONCAT(STACK_ITEM_MEMORY_POOL_NAME, _##name)

typedef struct {
    _Atomic STACK_HEAD head;
    atomic_size_t size;
    bool own_pool;
    STACK_ITEM_MEMORY_POOL_NAME *pool;
} STACK_NAME;


static bool STACK_FUNC(init_pool)(STACK_NAME *list, STACK_ITEM_MEMORY_POOL_NAME *pool) {
    if (list == NULL || pool == NULL) return false;
    list->pool = pool;
    list->own_pool = false;
    STACK_HEAD head = (STACK_HEAD){0, (STACK_NODE *)NULL};
    atomic_init(&list->head, head);
    atomic_init(&list->size, 0);
    return true;
}

static bool STACK_FUNC(init)(STACK_NAME *list) {
    if (list == NULL) return false;
    STACK_ITEM_MEMORY_POOL_NAME *pool = STACK_ITEM_MEMORY_POOL_FUNC(new)();
    if (pool == NULL) return false;
    if (!STACK_FUNC(init_pool)(list, pool)) {
        STACK_ITEM_MEMORY_POOL_FUNC(destroy)(pool);
        return false;
    }
    list->own_pool = true;
    return true;
}

static STACK_NAME *STACK_FUNC(new_pool)(STACK_ITEM_MEMORY_POOL_NAME *pool) {
    STACK_NAME *list = STACK_MALLOC(sizeof(STACK_NAME));
    if (list == NULL) return NULL;
    if (!STACK_FUNC(init_pool)(list, NULL)) {
        STACK_FREE(list);
        return NULL;
    }
    return list;
}

static STACK_NAME *STACK_FUNC(new)(void) {
    STACK_NAME *list = STACK_MALLOC(sizeof(STACK_NAME));
    if (list == NULL) return NULL;
    if (!STACK_FUNC(init)(list)) {
        STACK_FREE(list);
        return NULL;
    }
    return list;
}

bool STACK_FUNC(push)(STACK_NAME *list, STACK_TYPE value) {
    STACK_NODE *node = STACK_ITEM_MEMORY_POOL_FUNC(get)(list->pool);
    if (node == NULL) return false;
    node->value = value;
    STACK_HEAD old_head, new_head;
    do {
        old_head = atomic_load(&list->head);
        node->next = old_head.node;
        new_head.version = old_head.version + 1;
        new_head.node = node;
    } while (!atomic_compare_exchange_weak(&list->head, &old_head, new_head));
    atomic_fetch_add(&list->size, 1);
    return true;
}

bool STACK_FUNC(pop)(STACK_NAME *list, STACK_TYPE *result) {
    if (list == NULL || result == NULL) return false;
    STACK_HEAD old_head, new_head;
    do {
        old_head = atomic_load(&list->head);
        if (old_head.node == NULL) return false;
        // Only need the incremented version on one side, so we use push
        new_head.version = old_head.version;
        new_head.node = old_head.node->next;
    } while (!atomic_compare_exchange_weak(&list->head, &old_head, new_head));
    STACK_NODE *node = old_head.node;
    *result = node->value;
    STACK_ITEM_MEMORY_POOL_FUNC(release)(list->pool, node);
    atomic_fetch_sub(&list->size, 1);
    return true;
}

bool STACK_FUNC(release_node)(STACK_NAME *list, STACK_NODE *node) {
    if (list == NULL || node == NULL) return false;
    STACK_ITEM_MEMORY_POOL_FUNC(release)(list->pool, node);
    return true;
}

STACK_NODE *STACK_FUNC(pop_all)(STACK_NAME *list) {
    if (list == NULL) return NULL;
    STACK_NODE *result = NULL;
    STACK_HEAD old_head, new_head;
    size_t size;
    do {
        old_head = atomic_load(&list->head);
        size = atomic_load(&list->size);
        if (old_head.node == NULL) return NULL;
        new_head.version = old_head.version;
        new_head.node = NULL;
    } while (!atomic_compare_exchange_weak(&list->head, &old_head, new_head));
    result = old_head.node;
    atomic_fetch_sub(&list->size, size);
    return result;
}

bool STACK_FUNC(peek)(STACK_NAME *list, STACK_TYPE *result) {
    if (list == NULL) return false;
    STACK_HEAD head = atomic_load(&list->head);
    if (head.node == NULL) {
        return false;
    } else {
        *result = head.node->value;
    }
    return true;
}

size_t STACK_FUNC(size)(STACK_NAME *list) {
    if (list == NULL) return 0;
    return atomic_load(&list->size);
}

void STACK_FUNC(destroy)(STACK_NAME *list) {
    if (list == NULL) return;
    if (list->pool != NULL && list->own_pool) {
        STACK_ITEM_MEMORY_POOL_FUNC(destroy)(list->pool);
    }
    STACK_FREE(list);
}