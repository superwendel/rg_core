# rg_atomic

`rg_atomic.h` provides a minimal set of atomic operations on `u32`, `u64`, and
`void*` values: acquire loads, release stores, fetch-add, exchange,
compare-and-swap, and a full fence. It exists so that queue, reference-count,
and publish/consume code can be written once for MSVC, GCC, and Clang.

## Integration

```c
#include "rg_atomic.h"

typedef struct SharedCounter
{
	u32 refs;
	u32 ready;
} SharedCounter;

static void producer(SharedCounter* shared)
{
	rg_atomic_inc_u32(&shared->refs);
	/* ... fill data ... */
	rg_atomic_store_release_u32(&shared->ready, 1u);
}

static int consumer(SharedCounter* shared)
{
	if (!rg_atomic_load_acquire_u32(&shared->ready))
		return 0;
	if (rg_atomic_dec_u32(&shared->refs) == 0u)
	{
		/* last reference */
	}
	return 1;
}
```

## API

| Function | Behavior |
|---|---|
| `rg_atomic_load_acquire_u32/u64/ptr` | Acquire load |
| `rg_atomic_store_release_u32/u64/ptr` | Release store |
| `rg_atomic_fetch_add_u32/u64` | Sequentially consistent add; returns the previous value |
| `rg_atomic_inc_u32/u64`, `rg_atomic_dec_u32/u64` | Add or subtract one; returns the new value |
| `rg_atomic_exchange_u32` | Sequentially consistent swap; returns the previous value |
| `rg_atomic_cas_u32/u64/ptr` | Compare-and-swap; returns 1 when the value was replaced |
| `rg_atomic_fence` | Full memory fence |

## Configuration

None. The implementation is selected by `RG_COMPILER_MSVC`, `RG_COMPILER_GCC`,
and `RG_COMPILER_CLANG` from `rg_defs.h`.

## Thread safety

Every function is safe to call concurrently on the same naturally aligned
object. On MSVC x86/x64 the acquire load and release store are plain volatile
accesses behind a compiler barrier, which is sufficient on that memory model;
read-modify-write operations use the Interlocked intrinsics.

## Performance

The helpers compile to single instructions on x64 (`lock xadd`, `lock cmpxchg`,
`xchg`). `rg_atomic_fence` is `mfence` and should be rare.
