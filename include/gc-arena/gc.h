#pragma once
#include "gc-arena/arena.h"

#if __STDC_VERSION__ < 202311L
#define typeof __typeof__
#endif

typedef struct SWL_GCArena {
    SWL_Arena from_space;
    SWL_Arena to_space;
    size_t starting_capacity;
    bool space_phase;
} SWL_GCArena;

typedef size_t (*SWL_GCMoveFn)(SWL_GCArena*, void*);
typedef void (*SWL_GCMoveRootsFn)(SWL_GCArena*, void*);

typedef struct SWL_GCHeader {
    uintptr_t tagged_move_fn;
    char payload[0];
} SWL_GCHeader;


SWL_GCArena swl_gc_new(size_t starting_capacity);

void* swl_gc_alloc(SWL_GCArena* gc, size_t bytes, size_t alignment, SWL_GCMoveFn move_fn);

void* swl_gc_move(SWL_GCArena* gc, void* ptr, size_t bytes, size_t alignment);

void swl_gc_collect(SWL_GCArena* gc, void* roots, SWL_GCMoveRootsFn move_roots);

size_t swl_gc_used_space(SWL_GCArena* gc);
