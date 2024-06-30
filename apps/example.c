#include <stdio.h>
#include <stdalign.h>
#include "gc-arena/gc.h"

typedef struct MyAppState {
    size_t count;
    int* foo;
    int* bar;
} MyAppState;

void move_my_app_state(SWL_GCArena* gc, void* ptr) {
    MyAppState* s = ptr;
    s->foo = swl_gc_move(gc, s->foo, sizeof(int) * s->count, alignof(int));
    s->bar = swl_gc_move(gc, s->bar, sizeof(int) * s->count, alignof(int));
}

int main(int argc, char** argv) {
    SWL_GCArena gc = swl_gc_new(4096);
    MyAppState app_state = {300, NULL, NULL};
    app_state.foo = swl_gc_alloc(&gc, sizeof(int) * app_state.count, alignof(int), NULL);
    app_state.bar = swl_gc_alloc(&gc, sizeof(int) * app_state.count, alignof(int), NULL);
    swl_gc_alloc(&gc, sizeof(int) * 1024, alignof(int), NULL);
    printf("Used size: %zu\n", swl_gc_used_space(&gc));
    swl_gc_collect(&gc, &app_state, move_my_app_state);
    printf("Used size: %zu\n", swl_gc_used_space(&gc));
}