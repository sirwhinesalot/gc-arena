#pragma once
#include <stdlib.h>
#include <stdbool.h>

#ifndef SWL_PAGE_SIZE
#define SWL_PAGE_SIZE 16384
#endif

uintptr_t swl_align(uintptr_t address, size_t alignment);

typedef struct SWL_Chunk {
    struct SWL_Chunk* prev;
    struct SWL_Chunk* next;
    char* bottom;
    char* top;
    char* ceiling;
} SWL_Chunk;

size_t swl_chunk_capacity(SWL_Chunk* chunk);

size_t swl_chunk_used_space(SWL_Chunk* chunk);

bool swl_chunk_contains(SWL_Chunk* chunk, void* ptr);

SWL_Chunk* swl_chunk_new(size_t capacity, SWL_Chunk* prev, size_t alignment);

typedef struct SWL_Arena {
    SWL_Chunk* chunk;
} SWL_Arena;

SWL_Arena swl_arena_new(size_t capacity);

void* swl_arena_alloc(SWL_Arena* arena, size_t bytes, size_t alignment);

void swl_arena_free(SWL_Arena* arena);

void* swl_arena_snapshot(SWL_Arena* arena);

void swl_arena_rewind(SWL_Arena* arena, void* snapshot);

size_t swl_arena_used_space(SWL_Arena* arena);