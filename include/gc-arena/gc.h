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
    size_t allocation_size;
    SWL_GCMoveCallback move_children;
} SWL_GCHeader;

void swl_gc_header_set_space_bit(SWL_GCHeader* header, bool space_phase);

void swl_gc_header_set_align_bit(SWL_GCHeader* header, bool align_to_32_bytes);

bool swl_gc_header_in_current_space(SWL_GCHeader* header, bool space_phase);

bool swl_gc_header_is_32_byte_aligned(SWL_GCHeader* header);

void swl_gc_header_set_forwarded_ptr(SWL_GCHeader* from, void* to);

void* swl_gc_header_get_forwarded_ptr(SWL_GCHeader* header);

void* swl_gc_header_get_payload(SWL_GCHeader* header);

SWL_GCArena swl_gc_new(size_t starting_capacity);

SWL_GCHeader* swl_gc_get_header(void* ptr, size_t alignment);

void* swl_gc_alloc(SWL_GCArena* gc, size_t bytes, size_t alignment, SWL_GCMoveCallback move_children);

void* swl_gc_move(SWL_GCArena* gc, void* ptr, size_t bytes, size_t alignment);

void swl_gc_swap_spaces(SWL_GCArena* gc);

void swl_gc_collect(SWL_GCArena* gc, void* roots, SWL_GCMoveCallback move_roots);

size_t swl_gc_used_space(SWL_GCArena* gc);

typedef struct SWL_GCRoot {
    size_t alignment;
    void** pointer;
} SWL_GCRoot;

// array of pointers to garbage collected data in globals or the stack
typedef struct SWL_GCRootArray {
    size_t count;
    SWL_GCRoot* roots;
} SWL_GCRootArray;

void swl_move_root_array(SWL_GCArena* gc, void* data);

#define SWL_ROOT(P) (SWL_GCRoot){alignof(typeof(*P)), (void**)&P}
#define _SWL_ROOT_ARRAY(...) &(SWL_GCRootArray){sizeof((SWL_GCRoot[]){__VA_ARGS__})/sizeof(SWL_GCRoot), (SWL_GCRoot*)&(SWL_GCRoot[]){__VA_ARGS__}}
#define SWL_GC_COLLECT(GC, ...) swl_gc_collect(GC, _SWL_ROOT_ARRAY(__VA_ARGS__), swl_move_root_array)
