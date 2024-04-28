#include "gc-arena/arena.h"

static uintptr_t align(uintptr_t address, size_t alignment) {
    return (address + (alignment - 1)) & -alignment;
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

SWL_Chunk* swl_chunk_new(size_t capacity, SWL_Chunk* prev) {
    SWL_Chunk* chunk = malloc(sizeof(SWL_Chunk) + capacity);
    if (!chunk) { 
        abort(); 
    }
    if (prev) { 
        prev->next = chunk; 
    }
    chunk->prev = prev;
    chunk->next = NULL;
    chunk->bottom = (char*)chunk + sizeof(SWL_Chunk);
    chunk->top = chunk->bottom;
    chunk->ceiling = chunk->bottom + capacity;
    return chunk;
}

SWL_Arena swl_arena_new(size_t capacity) {
    return (SWL_Arena){swl_chunk_new(capacity, NULL)};
}

void* swl_arena_alloc(SWL_Arena* arena, size_t bytes, size_t alignment) {
    arena->chunk->top = (char*)align((uintptr_t)arena->chunk->top, alignment);
    if (arena->chunk->top + bytes >= arena->chunk->ceiling) {
        size_t double_capacity = (arena->chunk->ceiling - arena->chunk->bottom) * 2;
        size_t new_capacity = bytes > double_capacity ? align(bytes, SWL_PAGE_SIZE) : double_capacity;
        SWL_Chunk* next = swl_chunk_new(new_capacity, arena->chunk);
        arena->chunk = next;
    }
    void* result = arena->chunk->top;
    arena->chunk->top += bytes;
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