#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "threading/threading.h"
#include "greatest/greatest.h"

#define STACK_NAME stack_uint32
#define STACK_TYPE uint32_t
#include "concurrent_stack.h"
#undef STACK_NAME
#undef STACK_TYPE


typedef struct {
    uint64_t i;
    double x[3];
} id_point_t;

#define NUM_THREADS 8
#define NUM_PUSHES 500

int stack_thread(void *arg) {
    stack_uint32 *list = (stack_uint32 *)arg;
    for (size_t i = 0; i < NUM_PUSHES; i++) {
        stack_uint32_push(list, i);
        if (i % 4 == 0) {
            uint32_t val;
            if (!stack_uint32_pop(list, &val)) {
                return 1;
            }
        }
    }
    return 0;
}

TEST test_stack_multithreaded(void) {
    stack_uint32 *list = stack_uint32_new();

    thrd_t threads[NUM_THREADS];
    for (size_t i = 0; i < NUM_THREADS; i++) {
        thrd_create(&threads[i], stack_thread, list);
    }
    for (size_t i = 0; i < NUM_THREADS; i++) {
        thrd_join(threads[i], NULL);
    }
    size_t size = atomic_load(&list->size);
    ASSERT_EQ(size, NUM_THREADS * NUM_PUSHES * 3 / 4);
    size_t pops = 0;
    size_t prev_size = size;
    uint32_t value;
    ASSERT(stack_uint32_peek(list, &value));
    stack_uint32_node *node = stack_uint32_pop_all(list);
    while (node) {
        stack_uint32_node *next = node->next;
        stack_uint32_release_node(list, node);
        pops++;
        node = next;
    }
    size = atomic_load(&list->size);
    ASSERT_EQ(size, 0);
    ASSERT_EQ(pops, prev_size);
    ASSERT_FALSE(stack_uint32_pop(list, &value));
    stack_uint32_destroy(list);
    PASS();
}

/* Add definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();      /* command-line options, initialization. */

    RUN_TEST(test_stack_multithreaded);

    GREATEST_MAIN_END();        /* display results */
}