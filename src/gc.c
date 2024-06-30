#include <stdalign.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "gc-arena/gc.h"



static inline SWL_GCMoveFn swl_gc_header_get_move_fn(SWL_GCHeader* header) {
    assert(header->tagged_move_fn != UINTPTR_MAX);
    return !(header->tagged_move_fn & 2) ? (SWL_GCMoveFn)(header->tagged_move_fn & ~7) : NULL;
}

static inline size_t swl_gc_header_get_size(SWL_GCHeader* header) {
    assert(header->tagged_move_fn != UINTPTR_MAX);
    assert(header->tagged_move_fn & 2);
    return header->tagged_move_fn & ~7;
}

static inline void swl_gc_header_set_space_bit(SWL_GCHeader* header, bool space_phase) {
    assert(header->tagged_move_fn != UINTPTR_MAX);
    header->tagged_move_fn |= space_phase;
}

static inline bool swl_gc_header_in_current_space(SWL_GCHeader* header, bool space_phase) {
    assert(header->tagged_move_fn != UINTPTR_MAX);
    return space_phase == (header->tagged_move_fn & 1);
}

static inline void swl_gc_header_set_forwarded_ptr(SWL_GCHeader* from, void* to) {
    // pointers must have a minimum alignment of 8 due to the header, so the lower bits are 0.
    from->tagged_move_fn = UINTPTR_MAX;
    memcpy(from->payload, to, sizeof(void*));
}

static inline void* swl_gc_header_get_forwarded_ptr(SWL_GCHeader* header) {
    return header->tagged_move_fn == UINTPTR_MAX ? (void*)(header->payload) : NULL;
}

static inline SWL_GCHeader* swl_gc_get_header(void* ptr) {
    return (SWL_GCHeader*)((char*)ptr - 8);
}

static inline void swl_gc_swap_spaces(SWL_GCArena* gc) {
    SWL_Arena temp = gc->from_space;
    gc->from_space = gc->to_space;
    gc->to_space = temp;
}

SWL_GCArena swl_gc_new(size_t starting_capacity) {
    SWL_Arena from_space = swl_arena_new(starting_capacity);
    SWL_Arena to_space = {0};
    return (SWL_GCArena){from_space, to_space, starting_capacity, 0};
}

void* swl_gc_alloc(SWL_GCArena* gc, size_t bytes, size_t alignment, SWL_GCMoveFn move_children) {
    // 8 bytes for the header
    size_t aligned_bytes = swl_align(8 + bytes, alignment);
    char* alloc = swl_arena_alloc(&gc->from_space, aligned_bytes, alignment);
    // ensure padding bits are 0
    if (alignment > 8)
        memset(alloc, 0, alignment);
    // jump to right behind the alignment spot and place the header there
    SWL_GCHeader* header = (SWL_GCHeader*)(alloc + alignment - 8);
    // if no move function is given, we store the aligned size directly, with the second bit set
    // the second bit indicates if it is a function pointer, or a size
    if (move_children == NULL) {
        header->tagged_move_fn = aligned_bytes | 2;
    }
    else {
        header->tagged_move_fn = (uintptr_t)move_children;
    }
    swl_gc_header_set_space_bit(header, gc->space_phase);
    return (void*)(header->payload);
}

void* swl_gc_move(SWL_GCArena* gc, void* ptr, size_t bytes, size_t alignment) {
    size_t aligned_bytes = swl_align(bytes, alignment);
    SWL_GCHeader* old_header = swl_gc_get_header(ptr);
    if (swl_gc_header_in_current_space(old_header, gc->space_phase)) {
        printf("IN CURRENT SPACE, INVALID FLIP :(\n");
        return ptr;
    }
    void* forwarded = swl_gc_header_get_forwarded_ptr(ptr);
    if (forwarded) {
        return forwarded;
    }
    void* old_payload = old_header->payload;
    void* new_payload = swl_gc_alloc(gc, aligned_bytes, alignment, swl_gc_header_get_move_fn(old_header));
    memcpy(new_payload, old_payload, aligned_bytes);
    swl_gc_header_set_forwarded_ptr(old_header, new_payload);
    return new_payload;
}

void swl_gc_collect(SWL_GCArena* gc, void* roots, SWL_GCMoveRootsFn move_roots) {
    // flip the expected space
    gc->space_phase = gc->space_phase ? 0 : 1;
    gc->to_space = swl_arena_new(gc->starting_capacity);
    swl_gc_swap_spaces(gc);
    SWL_Chunk* scan_chunk = gc->from_space.chunk;
    char* scan_head = (char*)gc->from_space.chunk->bottom;
    move_roots(gc, roots);
    // iterate until we've copied every live pointer
    while (scan_chunk->next != NULL && scan_head < scan_chunk->top) {
        // 0 words are padding due to alignment
        while (*((uintptr_t*)scan_head) == 0) {
            scan_head += sizeof(uintptr_t);
        }
        // "move" members
        SWL_GCHeader* header = (SWL_GCHeader*)scan_head;
        scan_head += sizeof(SWL_GCHeader);
        if ((header->tagged_move_fn & ~7) != 0) {
            SWL_GCMoveFn mfn = swl_gc_header_get_move_fn(header);
            scan_head += mfn ? mfn(gc, header->payload) : swl_gc_header_get_size(header);
        }
        // did we go over?
        if (scan_head > scan_chunk->top) {
            scan_chunk = scan_chunk->next;
            scan_head = scan_chunk->bottom;
        }
    }
    // we could add an alternative version of arena_free that doesn't free the first chunk
    swl_arena_free(&gc->to_space);
}

size_t swl_gc_used_space(SWL_GCArena* gc) {
    return swl_arena_used_space(&gc->from_space);
}