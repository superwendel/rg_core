# rg_hash

`rg_hash.h` provides deterministic 64-bit hashing and arena-backed, typed
robin-hood hash maps and sets. It is a single-header C library built on
[`rg_mem.h`](rg_mem.md).

## Setup

Include `rg_hash.h`, initialize the global memory pool, and create an arena for
the table storage:

```c
#include "rg_hash.h"

rg_malloc(MB(64));
RgArena arena = rg_arena_create(MB(16));
```

All generated functions are `static inline`, so the header works naturally in
unity builds. Normal header guards also make repeated inclusion harmless. The
one exception is table growth: each table's rehash function is generated as a
`static` non-inlined (cold) function, so a `put` or `insert` call site inlines
only the probe loop and shares the growth code with every other call site of
that table type.

## Hash functions

```c
uint64_t rg_hash_bytes(const void* data, size_t len, uint64_t seed);
uint64_t rg_hash_str(const char* str);
uint64_t rg_hash_u32(uint32_t value);
uint64_t rg_hash_u64(uint64_t value);
uint64_t rg_hash_ptr(const void* ptr);
uint64_t rg_hash_u32_fast(uint32_t value);
uint64_t rg_hash_u64_fast(uint64_t value);
uint64_t rg_hash_ptr_fast(const void* ptr);
```

`rg_hash_bytes` implements xxHash64 and produces the same result on
little-endian and big-endian targets. A null data pointer is valid only when
`len` is zero. `rg_hash_str` uses `RG_HASH_SEED` and expects a non-null,
null-terminated string.

`rg_hash_u32`, `rg_hash_u64`, and `rg_hash_ptr` run a full 64-bit avalanche.
Use them for values that are stored or compared across runs, such as
identifiers. The `_fast` variants use one multiply instruction and are the
recommended hash functions for integer and pointer keys in the maps and sets
below; on this header's benchmark they cut the cost of every table operation
by 10 to 67 percent against the avalanche hash, typically 20 to 35 percent.
Their output is not a stable identifier.

`rg_hash_u32_fast` returns the high half of a 32x64 product, which every input
bit influences. `rg_hash_u64_fast` XORs the high and low halves of the full
128-bit product with an odd constant (`_umul128` on MSVC x64, `__int128` on
GCC and Clang, four 32-bit multiplies elsewhere; `RG_HASH_FORCE_PORTABLE_MUL128`
selects the portable form). A plain 64-bit multiply, with or without
xor-shift folds, carries information upward only, so keys that differ in
their upper bits share low bits and collapse into one probe sequence; the
128-bit product's high half depends on every input bit. The tests check both
functions against a random-key baseline on structured patterns: differences
only above bit 32, upper and middle bits moving together, packed float pairs,
generation/index identifiers, aligned addresses, and sequential and strided
integers, plus the resulting probe length in a real table.

Equality helpers are provided for common key types:

- `rg_hash_eq_u32`
- `rg_hash_eq_u64`
- `rg_hash_eq_ptr`
- `rg_hash_eq_str`

## Hash maps

Define a map type at file scope:

```c
RG_HASH_MAP_DEFINE(uint32_t, float, U32FloatMap,
                   rg_hash_u32_fast, rg_hash_eq_u32);
```

Then initialize and use it:

```c
U32FloatMap map;
rg_hash_map_init(U32FloatMap, &map, &arena);

if (!rg_hash_map_reserve(U32FloatMap, &map, 1024))
{
    // The arena could not provide enough storage.
}

int is_new = 0;
rg_hash_map_put(U32FloatMap, &map, 42u, 3.14f, &is_new);

float* value = rg_hash_map_get_ptr(U32FloatMap, &map, 42u);

int removed = 0;
rg_hash_map_remove(U32FloatMap, &map, 42u, &removed);
```

Map operations:

- `rg_hash_map_init`, `rg_hash_map_clear`, `rg_hash_map_free`
- `rg_hash_map_reserve`
- `rg_hash_map_put`, `rg_hash_map_try_put`, `rg_hash_map_remove`
- `rg_hash_map_get_ptr`, `rg_hash_map_get_ptr_const`
- `rg_hash_map_contains`
- `rg_hash_map_count`, `rg_hash_map_capacity`, `rg_hash_map_is_empty`

`rg_hash_map_reserve` returns nonzero on success and zero when the requested
capacity cannot be represented or the arena cannot satisfy the allocation.
`rg_hash_map_try_put` returns `1` for an insertion, `0` for an update, and `-1`
for allocation failure. The regular `rg_hash_map_put` wrapper writes only the
inserted/not-inserted result. Its optional `out_is_new` argument and the
`out_removed` argument are `int*`; pass `NULL` when a result is not needed.

## Hash sets

Define and use a set in the same way:

```c
RG_HASH_SET_DEFINE(uint64_t, EntitySet, rg_hash_u64, rg_hash_eq_u64);

EntitySet entities;
rg_hash_set_init(EntitySet, &entities, &arena);
rg_hash_set_insert(EntitySet, &entities, entity_id, NULL);

if (rg_hash_set_contains(EntitySet, &entities, entity_id))
{
    // Found it.
}
```

Set operations:

- `rg_hash_set_init`, `rg_hash_set_clear`, `rg_hash_set_free`
- `rg_hash_set_reserve`
- `rg_hash_set_insert`, `rg_hash_set_try_insert`, `rg_hash_set_remove`
- `rg_hash_set_contains`
- `rg_hash_set_count`, `rg_hash_set_capacity`, `rg_hash_set_is_empty`

`rg_hash_set_try_insert` returns `1` for a new key, `0` when the key already
exists, and `-1` for allocation failure.

Updating an existing map key or inserting an existing set key does not allocate,
including when the table is at its growth threshold. If allocation for a new key
fails, existing entries remain unchanged.

Every operation calls the hash function exactly once per key: `put` and
`insert` reuse the hash computed for the lookup when they store a new entry,
and growth reinserts entries from their stored hashes without rehashing keys.
This matters for string and byte-buffer keys, where hashing dominates the
operation. The generated `rg_hash_map_find_index_hashed_<name>(map, key, hash)`
and `rg_hash_set_find_index_hashed_<name>(set, key, hash)` functions expose the
same lookup for callers that already hold the prepared hash; the hash must be
the table's hash function output passed through `rg_hash_prepare`.

## Arena ownership and pointer lifetime

Growing a table allocates a replacement array from its arena. The old array is
not reclaimed until the arena is reset or released. Reserving the expected
entry count up front avoids those abandoned growth allocations.

`rg_hash_map_clear` and `rg_hash_set_clear` retain the current allocation for
reuse. The `free` operations only detach the table from its arena; they do not
reclaim arena memory.

Pointers returned by `rg_hash_map_get_ptr` can be invalidated by insertion,
reservation, or removal. Keys and values are copied into table entries. Pointer
and string keys remain owned by the caller, so their referenced storage must
remain valid and string contents must not change while stored.

## Configuration

Define options before including the header:

```c
#define RG_HASH_ASSERT(condition)  // Custom assertion macro
#define RG_HASH_MIN_CAP            // Minimum capacity; default: 8
#define RG_HASH_MAX_LOAD_NUM       // Load numerator; default: 7
#define RG_HASH_MAX_LOAD_DEN       // Load denominator; default: 10
#define RG_HASH_SEED               // Default string/integer seed; default: 0
#define RG_HASH_INVALID            // Invalid index sentinel; default: SIZE_MAX
```

The load numerator and denominator must describe a value strictly between zero
and one. Hash value zero is reserved for empty slots and is remapped internally,
so custom hash functions may still return zero.

## Performance

Measured with `benchmarks/bench_hash.cpp` on Linux x86-64 (Intel Core
i3-1005G1, GCC 13.3, `-O2`, pinned to one CPU, median of three process runs,
nanoseconds per operation). `std::unordered_map` uses the same reservation
policy, `max_load_factor(0.7)` then `reserve(count)`, but its prime bucket
count leaves it fuller than the power-of-two table: at 4,096 entries it is
68.8 percent occupied against rg's 50 percent. Every timed pass is checksummed
against the expected result, samples are calibrated to at least 20 ms, and
the implementation order rotates between samples.

| Entries | Operation | rg (`rg_hash_u32_fast`) | rg (`rg_hash_u32`) | `std::unordered_map` |
| ---: | --- | ---: | ---: | ---: |
| 4,096 | insert | 9.4 | 11.0 | 53.3 |
| 4,096 | find, insertion order | 4.5 | 6.7 | 8.0 |
| 4,096 | find, shuffled order | 4.2 | 6.3 | 8.8 |
| 4,096 | find, missing keys | 4.2 | 12.9 | 6.3 |
| 4,096 | remove | 9.5 | 13.4 | 42.4 |
| 65,536 | find, insertion order | 16.3 | 28.6 | 16.7 |
| 65,536 | find, shuffled order | 19.2 | 26.8 | 39.5 |
| 500,000 | insert | 51.5 | 64.0 | 142.0 |
| 500,000 | find, insertion order | 35.3 | 46.1 | 32.9 |
| 500,000 | find, shuffled order | 34.1 | 41.1 | 75.2 |
| 500,000 | find, missing keys | 25.4 | 50.1 | 48.0 |
| 500,000 | remove | 48.0 | 57.7 | 312.0 |

Against the avalanche hash, the fast hash was faster in all 30 size/operation
cells under both GCC and Clang: by 14 to 67 percent under GCC and 10 to 54
percent under Clang, with the largest gains on missing keys and the smallest
on 500,000-entry hits, where memory latency dominates. Against
`std::unordered_map`, the flat table leads shuffled lookups, missing keys,
insertion, and removal at every size under both compilers. `std::unordered_map`
leads lookups in exactly the insertion order at 500,000 entries under both
compilers (libstdc++ allocates nodes sequentially, so that access pattern
streams its node reads while the flat table's 16-byte entries are reached at
random), and under Clang it also leads the insertion-order lookups at 4,096
and 65,536 entries (6.5 vs 6.6 ns and 10.7 vs 23.8 ns), where its identity hash
and prime modulo compile very tightly. These runs were taken on a shared
laptop under other load, so absolute values vary between runs by tens of
percent; the directions did not. See the
[core benchmark report](benchmarks/rg_core.md#rg_hash) for methodology and the
earlier Windows results.

## Complexity and thread safety

Lookup, insertion, and removal are O(1) on average and O(n) under pathological
collisions. A map or set may be used by only one thread at a time unless access
is synchronized externally. Separate tables and arenas may be used concurrently.
