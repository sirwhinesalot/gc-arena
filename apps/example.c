#include <stdio.h>
#include <stdalign.h>
#include "gc-arena/gc.h"

int main(int argc, char** argv) {
    SWL_GCArena gc = swl_gc_new(4096);
    int* foo = swl_gc_alloc(&gc, 5000, alignof(int), NULL);
    swl_gc_alloc(&gc, 16000, alignof(int), NULL);
    int* bar = swl_gc_alloc(&gc, 3000, alignof(int), NULL);
    printf("used space before collection: %zu\n", swl_arena_used_space(&gc.from_space));
    SWL_GC_COLLECT(&gc, &foo, &bar);
    printf("used space after collection: %zu\n", swl_arena_used_space(&gc.from_space));
}