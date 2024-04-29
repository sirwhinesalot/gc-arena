#include <stdio.h>
#include <stdalign.h>
#include "gc-arena/gc.h"

int main(int argc, char** argv) {
    SWL_GCArena gc = swl_gc_new(4096);
    int* foo = swl_gc_alloc(&gc, sizeof(int) * 500, alignof(int), NULL);
    foo[0] = 1;
    foo[499] = 3;
    swl_gc_alloc(&gc, 16000, 16, NULL);
    int* bar = swl_gc_alloc(&gc, sizeof(int) * 300, alignof(int), NULL);
    bar[0] = 2;
    bar[299] = 4;
    printf("%d, %d, %d, %d\n", foo[0], foo[499], bar[0], bar[299]);
    printf("used space before collection: %zu\n", swl_arena_used_space(&gc.from_space));
    SWL_GC_COLLECT(&gc, SWL_ROOT(foo), SWL_ROOT(bar));
    printf("used space after collection: %zu\n", swl_arena_used_space(&gc.from_space));
    printf("%d, %d, %d, %d\n", foo[0], foo[499], bar[0], bar[299]);
}