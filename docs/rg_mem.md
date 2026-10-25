# rg_mem

`rg_mem.h` provides a global operating-system memory pool and lightweight bump
arenas for persistent, temporary, and per-frame allocations.

## Integration

```c
#include "rg_mem.h"

typedef struct GameState
{
	u64 frame_index;
} GameState;

typedef struct Position
{
	f32 x;
	f32 y;
	f32 z;
} Position;

int main(void)
{
	if (rg_malloc(MB(256)) != 0)
		return 1;

	RgArena persistent = rg_arena_create(MB(128));
	RgArena scratch = rg_arena_create(MB(4));

	GameState* state = RG_ARENA_PUSH_STRUCT(&persistent, GameState);
	Position* positions = RG_ARENA_PUSH_ARRAY(&scratch, Position, 1000);
	int allocations_succeeded = state != NULL && positions != NULL;

	rg_arena_reset(&scratch);
	rg_arena_free(&scratch);
	rg_arena_free(&persistent);
	rg_free();
	return allocations_succeeded ? 0 : 1;
}
```

Call `rg_malloc` once before creating arenas. Repeated calls are successful
no-ops until `rg_free` releases the pool. The pool size is rounded up to the
operating-system page size.

## Pool API

```c
int rg_malloc(size_t total_bytes);
void rg_free(void);
size_t rg_total(void);
size_t rg_used(void);
size_t rg_remaining(void);
```

`rg_used` includes arena capacity and any alignment between arenas. Destroying
an individual arena does not reclaim pool space; all pool memory is released
together by `rg_free`.

## Arena API

```c
RgArena rg_arena_create(size_t capacity);
void* rg_arena_alloc(RgArena* arena, size_t size);
void* rg_arena_alloc_aligned(RgArena* arena, size_t size, size_t alignment);
void* rg_arena_alloc_array(RgArena* arena, size_t element_size,
                           size_t count, size_t alignment);
void rg_arena_reset(RgArena* arena);
void rg_arena_decommit(RgArena* arena);
void rg_arena_free(RgArena* arena);
size_t rg_arena_remaining(const RgArena* arena);
```

Allocation returns `NULL` for zero-sized requests, invalid alignment, integer
overflow, or insufficient capacity. `RG_ARENA_PUSH_STRUCT` and
`RG_ARENA_PUSH_ARRAY` align allocations to the requested type; array-size
multiplication is checked for overflow.

`rg_arena_reset` makes all arena storage reusable. With `RG_MALLOC_SECURE`, it
first clears the used bytes. On lazy-commit platforms, `rg_arena_decommit`
releases committed pages while retaining the reserved address range.

## Configuration

Define options before including the header:

```c
#define RG_MALLOC_SECURE
#define RG_MALLOC_ASSERT(condition) my_assert(condition)
#define RG_MALLOC_DEFAULT_ALIGNMENT 16
#define RG_MALLOC_LAZY_COMMIT 1
#define RG_MALLOC_PREFAULT 0
#define RG_MALLOC_HUGE_PAGES 0
#include "rg_mem.h"
```

Lazy commit uses `VirtualAlloc` and is currently Windows-only. It defaults to
enabled on Windows. Linux and macOS use `mmap` with lazy commit disabled.

### First-touch cost

Freshly mapped pool memory is not backed by physical pages until it is first
written, so a 256 MB pool takes roughly 65,000 page faults spread over
whichever frames first touch each page. Two opt-in options (both `0` or `1`,
default `0`, validated with `#error`) move or shrink that cost without changing
any allocation behavior; pages are zero-filled either way.

`RG_MALLOC_PREFAULT 1` faults the whole pool in when it is obtained, so the
cost is paid once inside `rg_malloc` instead of during gameplay. On Linux this
is `MAP_POPULATE`; on macOS and Windows the pool is touched one byte per page.
With `RG_MALLOC_LAZY_COMMIT 1` (Windows) the pool is only reserved, so the
touch happens as each arena commits pages: `rg_arena_alloc` pays the faults for
the pages it commits rather than leaving them to the first write. The option
is independent of `RG_MALLOC_SECURE`, which only affects `rg_arena_reset`.

`RG_MALLOC_HUGE_PAGES 1` asks for transparent huge pages with
`madvise(MADV_HUGEPAGE)` on Linux: fewer faults on first touch and less TLB
pressure for large arenas. It is advisory; when THP is disabled or the kernel
cannot supply 2 MB pages, ordinary pages are used. When free memory is
fragmented the kernel may spend time compacting before the first touch
completes (`/sys/kernel/mm/transparent_hugepage/defrag` controls this), so
measure on the target machine. With both options set, the pool is touched
after the advice so the populated pages can be huge. Windows large pages
require `SeLockMemoryPrivilege` and macOS does not expose superpages through
`mmap`, so the option is a no-op there.

## Unity builds and threads

The pool state is `static`. A unity build has one shared pool, while separately
compiled translation units each receive independent pool state.

Initialize and release the pool, and create arenas, from one thread. An arena
can be used by one thread at a time; use separate arenas for concurrent work.
