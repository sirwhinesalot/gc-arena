# GC Arena
A simple Garbage Collected Arena for C.

A GC arena works mostly like a regular arena with two key differences:

- Every allocation has a 24 byte header prepended to it.
- It's possible to garbage collect every unreachable pointer in the arena.

With the exception of the extra header and the small amount of code needed to set it up, allocation is extremely fast.

De-allocation performance is proportional to the amount of live objects.
Feel free to allocate massive amounts of temporary data, the garbage collector won't even notice if there aren't any roots.

The garbage collector is a simple copying semi-space collector using [Cheney's algorithm](https://en.wikipedia.org/wiki/Cheney%27s_algorithm).

Garbage collection must be explicitly triggered by the user, the garbage collector will just keep allocating more memory until the system runs out. This restriction gives the user more control and avoids the need to dig trough the C stack for roots.

## Usage

Easiest way to get the library is to use FetchContent:

```cmake
include(FetchContent)

FetchContent_Declare(gc-arena GIT_REPOSITORY https://github.com/sirwhinesalot/gc-arena.git)
FetchContent_MakeAvailable(gc-arena)
```

Zipped releases will be provided at a later date.

You can import the library with: `#include "gc-arena/gc.h"`.
If you just want a basic (non-garbage-collected) arena, you can use: `#include "gc-arena/arena.h"`.

Working with a GCArena is pretty simple:

```c
SWL_GCArena gc = swl_gc_arena_new(<starting-capacity>);
int* foo = swl_gc_alloc(&gc, 5000, alignof(int), NULL);
int* bar = swl_gc_alloc(&gc, 16000, alignof(int), NULL);
int* baz = swl_gc_alloc(&gc, 3000, alignof(int), NULL);
// &foo and &baz are roots
SWL_GC_COLLECT(&gc, &foo, &baz);
// bar is gone, poof
```

SWL_GC_COLLECT is a convenience macro over the more general `swl_gc_collect` function. It takes in a pointer to a garbage collected arena and a list of pointers to roots, and frees everything that is not transitively reachable from those roots (the pointers are updated, hence needing the address of a pointer). For more complex use cases see `swl_gc_collect` which accepts a custom function.

The `NULL` argument to `swl_gc_alloc` in the example above is a pointer to a tracing function that moves the members/indexes of the datastructure that contain pointers to data in the GCArena, as otherwise GCArena does not know how to do so:

```c
typedef struct MyDataStruct {
    int* foo;
    int* bar;
}

void move_myds_members(SWL_GCArena* gc, void* ptr) {
    MyDataStruct* ds = ptr;
    *ds->foo = swl_gc_move(gc, ds->foo);
    *ds->bar = swl_gc_move(gc, ds->bar);
}

MyDataStruct* ds = swl_gc_alloc(&gc, sizeof(MyDataStruct), alignof(MyDataStruct), move_my_ds_members);
ds->foo = swl_gc_alloc(...);
ds->bar = swl_gc_alloc(...);
SWL_GC_COLLECT(&gc, &ds);
```

## Underlying Arena

GCArenas are backed by regular arenas. The current arena implementation uses a linked-list of malloc'ed chunks with no size limit.
Alternative implementations based on fixed-size arenas or using mmap are possible as well, but not yet implemented.

## TODO:

- Need to find a way to lower the 24 byte header to 16 bytes.
- Alignment-handling code is wrong, try to keep your data aligned to 8 bytes for now.
