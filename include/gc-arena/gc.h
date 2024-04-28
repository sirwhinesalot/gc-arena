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

typedef void (*SWL_GCMoveCallback)(SWL_GCArena*, void*);

typedef struct SWL_GCHeader {
    uintptr_t forwarded_ptr_and_tags;
    // todo: alignment over 8 bytes is a a problem...
    size_t allocation_size;
    SWL_GCMoveCallback move_children;
} SWL_GCHeader;

void swl_gc_header_set_space_bit(SWL_GCHeader* header, bool space_phase);

bool swl_gc_header_in_current_space(SWL_GCHeader* header, bool space_phase);

void swl_gc_header_set_forwarded_ptr(SWL_GCHeader* from, void* to);

void* swl_gc_header_get_forwarded_ptr(SWL_GCHeader* header);

void* swl_gc_header_get_payload(SWL_GCHeader* header);

SWL_GCArena swl_gc_new(size_t starting_capacity);

SWL_GCHeader* swl_gc_get_header(void* ptr);

void* swl_gc_alloc(SWL_GCArena* gc, size_t bytes, size_t alignment, SWL_GCMoveCallback move_children);

void* swl_gc_move(SWL_GCArena* gc, void* ptr, size_t bytes, size_t alignment);

void swl_gc_swap_spaces(SWL_GCArena* gc);

void swl_gc_collect(SWL_GCArena* gc, void* roots, SWL_GCMoveCallback move_roots);

size_t swl_gc_used_space(SWL_GCArena* gc);

// array of pointers to garbage collected data in globals or the stack
typedef struct SWL_GCRootArray {
    size_t count;
    void*** roots;
} SWL_GCRootArray;

void swl_move_root_array(SWL_GCArena* gc, void* data);

// by pure coincidence the ## GCC extension for empty __VA_ARGS__ also does the right thing in MSVC
#define _SWL_ARRAY_SIZE(R, ...) sizeof((typeof(R)[]){R, ##__VA_ARGS__})/sizeof(void*)
#define SWL_GC_ROOT_ARRAY(R, ...) (SWL_GCRootArray){_SWL_ARRAY_SIZE(R, ##__VA_ARGS__), (void***)(typeof(R)[]){R, ##__VA_ARGS__}}

#define SWL_GC_COLLECT(GC, R, ...) swl_gc_collect(GC, &SWL_GC_ROOT_ARRAY(R, ##__VA_ARGS__), swl_move_root_array)
