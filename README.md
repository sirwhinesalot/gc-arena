# GC Arena
A simple Garbage Collected Arena for C.

A GC arena works mostly like a regular arena with two key differences:

- Every allocation has an 8 byte header prepended to it and the minimum alignment is 8 bytes.
- It's possible to garbage collect every unreachable object in the arena.

With the exception of the extra header and the small amount of code needed to set it up, allocation is extremely fast, not as fast as a simple pointer bump but faster than malloc.

De-allocation performance is proportional to the amount of live objects. If there is are no live objects, calling collect will just clear the underlying arena. Feel free to allocate massive amounts of temporary data, the garbage collector won't even notice if it isn't reachable from a root.

If every object is live, then collection will be very slow as every object needs to be salvaged to a new arena. Old data needs to be kept around until collection is finished, so memory usage during collection is almost exactly 2x the amount of live data.

In the next version I will add support for non-moving allocations, which will dedicate an entire arena chunk just for that allocation. For those allocations "moving" is just transfering from one chunk list to another, the pointer remains stable and no bytes are copied. For large[^1] allocations this should give a nice speedup.

The garbage collector is a simple copying semi-space collector using [Cheney's algorithm](https://en.wikipedia.org/wiki/Cheney%27s_algorithm), except that this implementation uses a chunked arena implementation underneath rather than splitting the heap in half.

Garbage collection must be explicitly triggered by the user, the garbage collector will just keep allocating more memory until the system runs out. This restriction gives the user more control and avoids the need to dig trough the C stack for roots.

[^1]: Still need to figure out some good default sizes for the chunks and for what constitutes a "large" allocation. Either way you can request a "large allocation" of a single byte if you need the pointer to be stable.

## Usage

Easiest way to get the library is to use FetchContent:

```cmake
include(FetchContent)

FetchContent_Declare(gc-arena GIT_REPOSITORY https://github.com/sirwhinesalot/gc-arena.git GIT_SHALLOW 1)
FetchContent_MakeAvailable(gc-arena)
```

Zipped releases will be provided at a later date.

You can import the library with: `#include "gc-arena/gc.h"`.
If you just want a basic (non-garbage-collected) arena, you can use: `#include "gc-arena/arena.h"`.

Working with a GCArena is pretty simple:

```c
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
    // some temp data
    swl_gc_alloc(&gc, sizeof(int) * 1024, alignof(int), NULL);
    printf("Used size: %zu\n", swl_gc_used_space(&gc));
    swl_gc_collect(&gc, &app_state, move_my_app_state);
    // the temp allocation is gone, poof
    printf("Used size: %zu\n", swl_gc_used_space(&gc));
}
```

This example contains the entire API surface of GC arena.

- `swl_gc_new` creates a garbage collected arena with given number as the default chunk size in bytes. Chunk size doubles every time a new one is needed, unless the requested allocation size is larger than that. This will likely be changed in the future.
- `swl_gc_alloc` allocates the given number of bytes (+ some extra due to the header and alignment) on a GC Arena. The last parameter is a function that knows how to traverse the allocated object and move all of its GC pointers. Moving is handled by `swl_gc_move` explained below.
- `swl_gc_collect` calls the actual garbage collection algorithm on the GC arena, starting from the roots it moves every reachable pointer to a new arena (the moving involves a copy) and then frees the old arena. It takes as parameters a pointer to the roots and a pointer to a function that can traverse the roots. In the example above the roots are all stored in a single struct. Traversal is implemented through `swl_gc_move`, see below.
- `swl_gc_move` is the function that takes care of traversing and moving GC pointers. It should be called by a user defined function to adjust every GC pointer stored in an object, be it the roots or any other data structure that contains GC pointers. The only difference between an root traversal function and an object traversal function is the return value. For the roots there is none, the function is void, while for an object the function must return its size.
- `swl_gc_used_space` lets you know how much space is actually stored in the garbage collector. This does not include the free space in the arena chunks, only allocated data (including padding and the header).

> Note: the functions that call `swl_gc_move` should only move GC pointers that are direct members of the object and should *not* be called recursively through indirect GC pointers. Moving of the object and transitive moves are handled by the garbage collection algorithm itself.

## Underlying Arena

GCArenas are backed by regular arenas. The current arena implementation uses a linked-list of malloc'ed chunks.
Alternative implementations based on fixed-size arenas or using mmap are possible as well, but are not implemented for now.

## TODO

- API for managing shadow stacks to facilitate dealing with roots.
- Support other underlying arena types (e.g. fixed, mmap-based)
- Support for incremental GC (requires barriers, need to think how to best deal with those).
- Document the API properly.
- Tests and benchmarks, lots of them! Help very much accepted.
- Zipped releases on github.

## Changelog

- 0.3:
    - Reduction of the header size from 24 bytes to 8 bytes[^2]
    - Removal of the SWL_ROOT macro and SWL_GC_COLLECT macro, they're no longer necessary.
    - Header padding is no longer fixed to two allowed sizes, it's handled by skipping zeroed out words (headers are never 0)
- 0.2:
    - Added proper alignment support.
    - New SWL_ROOT macro that extracts alignment information from a root.
- 0.1:
    - First version 🥳.

[^2]: Thanks to a productive discussion on hackernews, originally I expected 16 would be the minimum.