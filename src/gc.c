#include <stdalign.h>
#include <string.h>
#include "gc-arena/gc.h"

#define MAX(A, B) ((A) >= (B) ? (A) : (B))

void swl_gc_header_set_space_bit(SWL_GCHeader* header, bool space_phase) {
    header->forwarded_ptr_and_tags = (uintptr_t)header | space_phase;
}

void swl_gc_header_set_align_bit(SWL_GCHeader* header, bool align_to_32_bytes) {
    header->forwarded_ptr_and_tags = (uintptr_t)header | (align_to_32_bytes << 1);
}

bool swl_gc_header_in_current_space(SWL_GCHeader* header, bool space_phase) {
    return space_phase == (header->forwarded_ptr_and_tags & 1);
}

bool swl_gc_header_is_32_byte_aligned(SWL_GCHeader* header) {
    return (header->forwarded_ptr_and_tags & 2);
}

void swl_gc_header_set_forwarded_ptr(SWL_GCHeader* from, void* to) {
    // pointers must have a minimum alignment of 8 due to the header, so the lower bits are 0.
    uintptr_t tags = from->forwarded_ptr_and_tags & 7;
    from->forwarded_ptr_and_tags = (uintptr_t)to | tags;
}

void* swl_gc_header_get_forwarded_ptr(SWL_GCHeader* header) {
    return (void*)(header->forwarded_ptr_and_tags & ~7);
}

void* swl_gc_header_get_payload(SWL_GCHeader* header) {
    return (void*)((uintptr_t)header + (swl_gc_header_is_32_byte_aligned(header) ? 32 : sizeof(SWL_GCHeader)));
}

SWL_GCArena swl_gc_new(size_t starting_capacity) {
    SWL_Arena from_space = swl_arena_new(starting_capacity);
    SWL_Arena to_space = {0};
    return (SWL_GCArena){from_space, to_space, starting_capacity, false};
}

SWL_GCHeader* swl_gc_get_header(void* ptr, size_t alignment) {
    size_t offset = swl_align(sizeof(SWL_GCHeader), alignment);
    return (SWL_GCHeader*)((uintptr_t)ptr - offset);
}

void* swl_gc_alloc(SWL_GCArena* gc, size_t bytes, size_t alignment, SWL_GCMoveCallback move_children) {
    //size_t tweaked_alignment = swl_align(sizeof(SWL_GCHeader), alignment);
    size_t tweaked_offset = swl_align(sizeof(SWL_GCHeader), alignment);
    size_t tweaked_bytes = swl_align(tweaked_offset + bytes, alignment);
    SWL_GCHeader* header = swl_arena_alloc(&gc->from_space, tweaked_bytes, alignment);
    swl_gc_header_set_space_bit(header, gc->space_phase);
    swl_gc_header_set_align_bit(header, alignment > 8);
    header->move_children = move_children;
    header->allocation_size = bytes;
    return (void*)((char*)header + tweaked_offset);
}

void* swl_gc_move(SWL_GCArena* gc, void* ptr, size_t bytes, size_t alignment) {
    SWL_GCHeader* old_header = swl_gc_get_header(ptr, alignment);
    if (swl_gc_header_in_current_space(old_header, gc->space_phase)) {
        return ptr;
    }
    void* forwarded = swl_gc_header_get_forwarded_ptr(ptr);
    if (forwarded) {
        return forwarded;
    }
    void* old_payload = swl_gc_header_get_payload(old_header);
    void* new_payload = swl_gc_alloc(gc, bytes, alignment, old_header->move_children);
    memcpy(new_payload, old_payload, bytes);
    swl_gc_header_set_forwarded_ptr(old_header, swl_gc_get_header(new_payload, alignment));
    return new_payload;
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
        size_t offset = swl_gc_header_is_32_byte_aligned(header) ? 32 : sizeof(SWL_GCHeader);
        scan_head += header->allocation_size + offset;
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
        SWL_GCRoot root = array->roots[i];
        SWL_GCHeader* header = swl_gc_get_header(*root.pointer, root.alignment);
        *root.pointer = swl_gc_move(gc, *root.pointer, header->allocation_size, root.alignment);
    }
}