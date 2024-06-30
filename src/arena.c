#include <assert.h>
#include "gc-arena/arena.h"

uintptr_t swl_align(uintptr_t number, size_t alignment) {
    if (alignment < 8) {
        alignment = 8;
    }
    assert(alignment % 8 == 0);
    return (number + alignment - 1) & -alignment;
}

size_t swl_chunk_capacity(SWL_Chunk* chunk) {
    return chunk->ceiling - chunk->bottom;
}

size_t swl_chunk_used_space(SWL_Chunk* chunk) {
    return chunk->top - chunk->bottom;
}

bool swl_chunk_contains(SWL_Chunk* chunk, void* ptr) {
    return (char*)ptr >= chunk->bottom && (char*)ptr < chunk->ceiling;
}

SWL_Chunk* swl_chunk_new(size_t capacity, SWL_Chunk* prev, size_t alignment) {
    size_t extended_capacity = swl_align(capacity + sizeof(SWL_Chunk), alignment);
    // the return malloc buffer might be misaligned by up to (alignment/8-1) * 8 bytes
    // store some extra bytes to allow for that shift
    SWL_Chunk* chunk = malloc(extended_capacity + alignment);
    if (!chunk) { 
        abort(); 
    }
    if (prev) { 
        prev->next = chunk; 
    }
    chunk->prev = prev;
    chunk->next = NULL;
    // ensure the chunks start aligned
    chunk->bottom = (char*)swl_align((uintptr_t)chunk + sizeof(SWL_Chunk), alignment);
    chunk->top = chunk->bottom;
    chunk->ceiling = chunk->bottom + swl_align(capacity, alignment);
    return chunk;
}

SWL_Arena swl_arena_new(size_t capacity) {
    // TODO: find a way to avoid this hardcoded alignment
    // perhaps the chunk is only requested on first allocation?
    return (SWL_Arena){swl_chunk_new(capacity, NULL, 8)};
}

void* swl_arena_alloc(SWL_Arena* arena, size_t bytes, size_t alignment) {
    size_t space_required = swl_align(bytes, alignment);
    if (arena->chunk->top + space_required >= arena->chunk->ceiling) {
        size_t double_capacity = (arena->chunk->ceiling - arena->chunk->bottom) * 2;
        // ensure we have enough space for the requested allocation
        size_t new_capacity = space_required > double_capacity ? space_required : double_capacity;
        SWL_Chunk* next = swl_chunk_new(new_capacity, arena->chunk, alignment);
        arena->chunk = next;
    }
    void* result = arena->chunk->top;
    arena->chunk->top += space_required;
    return result;
}

void swl_arena_free(SWL_Arena* arena) {
    while (arena->chunk) {
        SWL_Chunk* prev = arena->chunk->prev;
        free(arena->chunk);
        arena->chunk = prev;
    }
    arena->chunk = NULL;
}

void* swl_arena_snapshot(SWL_Arena* arena) {
    return arena->chunk->top;
}

void swl_arena_rewind(SWL_Arena* arena, void* snapshot) {
    while (!swl_chunk_contains(arena->chunk, snapshot)) {
        SWL_Chunk* prev = arena->chunk->prev;
        free(arena->chunk);
        prev->next = NULL;
        arena->chunk = prev;
    }
    arena->chunk->top = snapshot;
}

size_t swl_arena_used_space(SWL_Arena* arena) {
    SWL_Chunk* chunk = arena->chunk;
    size_t total_used_space = 0;
    while (chunk) { 
        total_used_space += swl_chunk_used_space(chunk);
        chunk = chunk->prev;
    }
    return total_used_space;
}