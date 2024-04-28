#include <stdalign.h>
#include "gc-arena/gc.h"

#define MAX(A, B) ((A) >= (B) ? (A) : (B))

void swl_gc_header_set_space_bit(SWL_GCHeader* header, bool space_phase) {
    uintptr_t ptr_bits = header->forwarded_ptr_and_tags & ~7;
    header->forwarded_ptr_and_tags = ptr_bits | space_phase;
}

bool swl_gc_header_in_current_space(SWL_GCHeader* header, bool space_phase) {
    return space_phase == (header->forwarded_ptr_and_tags & 1);
}

void swl_gc_header_set_forwarded_ptr(SWL_GCHeader* from, void* to) {
    // pointers must have a minimum alignment of 8 due to the header, so the lower bits are 0.
    // we want to keep the tag bits stored in them.
    from->forwarded_ptr_and_tags |= (uintptr_t)to;
}

void* swl_gc_header_get_forwarded_ptr(SWL_GCHeader* header) {
    return (void*)(header->forwarded_ptr_and_tags & ~3);
}

void* swl_gc_header_get_payload(SWL_GCHeader* header) {
    return (void*)((uintptr_t)header + sizeof(SWL_GCHeader));
}

SWL_GCArena swl_gc_new(size_t starting_capacity) {
    SWL_Arena from_space = swl_arena_new(starting_capacity);
    SWL_Arena to_space = {0};
    return (SWL_GCArena){from_space, to_space, starting_capacity, false};
}

SWL_GCHeader* swl_gc_get_header(void* ptr) {
    return (SWL_GCHeader*)((uintptr_t)ptr - sizeof(SWL_GCHeader));
}

void* swl_gc_alloc(SWL_GCArena* gc, size_t bytes, size_t alignment, SWL_GCMoveCallback move_children) {
    size_t tweaked_alignment = MAX(alignof(SWL_GCHeader), alignment);
    SWL_GCHeader* header = swl_arena_alloc(&gc->from_space, sizeof(SWL_GCHeader) + bytes, tweaked_alignment);
    swl_gc_header_set_space_bit(header, gc->space_phase);
    header->move_children = move_children;
    header->allocation_size = bytes;
    return (void*)((char*)header + sizeof(SWL_GCHeader));
}

void* swl_gc_move(SWL_GCArena* gc, void* ptr, size_t bytes, size_t alignment) {
    SWL_GCHeader* old_header = swl_gc_get_header(ptr);
    if (swl_gc_header_in_current_space(old_header, gc->space_phase)) {
        return ptr;
    }
    void* forwarded = swl_gc_header_get_forwarded_ptr(ptr);
    if (forwarded) {
        return forwarded;
    }
    void* payload = swl_gc_alloc(gc, bytes, alignment, old_header->move_children);
    swl_gc_header_set_forwarded_ptr(old_header, swl_gc_get_header(payload));
    return payload;
}

void swl_gc_swap_spaces(SWL_GCArena* gc) {
    SWL_Arena temp = gc->from_space;
    gc->from_space = gc->to_space;
    gc->to_space = temp;
}

void swl_gc_collect(SWL_GCArena* gc, void* roots, SWL_GCMoveCallback move_roots) {
    // flip the expected space
    gc->space_phase = gc->space_phase ? 0 : 1;
    gc->to_space = swl_arena_new(gc->starting_capacity);
    swl_gc_swap_spaces(gc);
    SWL_Chunk* scan_chunk = gc->from_space.chunk;
    char* scan_head = (char*)gc->from_space.chunk->bottom;
    move_roots(gc, roots);
    // iterate until we've copied every live pointer
    while (scan_chunk->next != NULL && scan_head < scan_chunk->top) {
        // "move" members
        SWL_GCHeader* header = (SWL_GCHeader*)scan_head;
        scan_head += header->allocation_size + sizeof(SWL_GCHeader);
        if (header->move_children) {
            header->move_children(gc, swl_gc_header_get_payload(header));
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

void swl_move_root_array(SWL_GCArena* gc, void* data) {
    SWL_GCRootArray* array = data;
    for (int i = 0; i < array->count; i++) {
        void* root = *array->roots[i];
        SWL_GCHeader* header = swl_gc_get_header(root);
        // TODO: fix alignment issue
        *array->roots[i] = swl_gc_move(gc, root, header->allocation_size, alignof(void*));
    }
}