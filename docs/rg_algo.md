# rg_algo

`rg_algo.h` generates type-safe sorting, selection, searching, and min/max
functions for caller-defined C types. It depends only on
[`rg_defs.h`](rg_defs.md) and performs no allocation.

## Setup

Define a strict less-than comparator, then instantiate the algorithms at file
scope:

```c
#include "rg_algo.h"

static int entity_less(const Entity* a, const Entity* b)
{
    return a->depth < b->depth;
}

RG_ALGO_DEFINE(Entity, EntityAlgo, entity_less);
```

The name argument becomes the suffix of every generated type and function.
Each generated function is `static inline`, so separate translation units and
unity builds do not share mutable state or require an implementation toggle.

Comparators must implement strict weak ordering. They should return nonzero
only when the first value belongs before the second value.

## Generated algorithms

`RG_ALGO_DEFINE(type, name, less_fn)` generates these primary operations:

| Operation | Generated function |
| --- | --- |
| Unstable sort | `rg_algo_sort_name(data, count)` |
| Stable sort | `rg_algo_stable_sort_name(data, count, scratch, scratch_count)` |
| Partial selection | `rg_algo_nth_element_name(data, count, nth)` |
| Sorted check | `rg_algo_is_sorted_name(data, count)` |
| Lower bound | `rg_algo_lower_bound_name(data, count, key)` |
| Upper bound | `rg_algo_upper_bound_name(data, count, key)` |
| Binary search | `rg_algo_binary_search_name(data, count, key)` |
| Minimum/maximum | `rg_algo_min_index_name` / `rg_algo_max_index_name` |
| Minimum and maximum | `rg_algo_minmax_index_name(data, count, out_min, out_max)` |

For the example above:

```c
Entity entities[256];
size_t entity_count = 0;

rg_algo_sort_EntityAlgo(entities, entity_count);
```

The regular sort recognizes already nondecreasing input and returns after one
linear scan. Nonincreasing input is reversed in place after the same scan.
Other inputs use a pattern-defeating quicksort. Pivots come from a median of
three, or a median of three medians above `RG_ALGO_NINTHER_CUTOFF` elements.
Large ranges are partitioned in blocks of `RG_ALGO_BLOCK` elements: each block
records the offsets of misplaced elements with branch-free comparisons and
then swaps them in bulk, so random data does not pay for mispredicted
branches. Ranges that the first scans find mostly in order, and ranges below
128 elements, use a conventional branching Hoare partition instead. Ranges of
elements equal to the previous pivot are recognized and skipped, so many
duplicates finish in linear time per level, and a partition that moves
nothing triggers a bounded insertion sort that finishes nearly sorted input
early. Insertion sort finishes small ranges, strongly unbalanced partitions
shuffle a few elements to break adversarial patterns, and heap sort provides
the worst-case fallback after too many unbalanced partitions. It is not
stable.

The stable sort uses insertion-sorted runs followed by stable merging. Scratch
must contain at least `count` elements and must not overlap the input:

```c
Entity scratch[256];
rg_algo_stable_sort_EntityAlgo(entities, entity_count,
                               scratch, RG_ARRAY_COUNT(scratch));
```

`rg_algo_nth_element_name` places the value that a complete sort would put at
`nth` into that position. Values before it do not compare greater and values
after it do not compare less, but neither side is otherwise sorted. It uses
the same pivot selection and partitions as the sort.

Search operations require sorted input. Binary search returns the first
equivalent element, or `RG_ALGO_INVALID` when the key is absent. Min/max
operations also return `RG_ALGO_INVALID` for an empty input.

Insertion sort and heap sort are also emitted as
`rg_algo_insertion_sort_name` and `rg_algo_heap_sort_name` when direct control
over the algorithm is useful.

## Radix sorting

The radix macros generate stable least-significant-digit sorts for unsigned
32-bit or 64-bit keys:

```c
typedef struct RenderItem
{
    i32 depth;
    u32 insertion_order;
} RenderItem;

static u32 render_item_key(const RenderItem* item)
{
    return (u32)item->depth ^ UINT32_C(0x80000000);
}

RG_ALGO_RADIX_U32_DEFINE(RenderItem, RenderItemRadix, render_item_key);

RenderItem items[1024];
RenderItem scratch[1024];
size_t item_count = 0;

rg_algo_radix_sort_RenderItemRadix(items, item_count,
                                   scratch, RG_ARRAY_COUNT(scratch));
```

XORing the sign bit maps signed two's-complement order into unsigned key order.
For naturally unsigned keys, return the key directly. Use
`RG_ALGO_RADIX_U64_DEFINE` for `u64` keys.

The key extractor should be deterministic and free of side effects because it
may be evaluated more than once per value. As with stable comparison sorting,
radix scratch must contain at least `count` elements and must not overlap the
input.

The sort reads the keys once to histogram the lowest digit, record which key
bits vary at all, and detect inputs that are already ascending (returned as
they are) or strictly descending (reversed in place; with no equal keys that is
still stable). Digits that never vary are skipped, so keys confined to a small
range or to a few bits cost one or two passes rather than four. Each scatter
pass histograms the next varying digit from the keys it is already reading, so
a sort with `d` varying digits touches the data `d + 1` times instead of
`2d`. Inputs of `RG_ALGO_RADIX_INSERTION_CUTOFF` elements or fewer are
insertion sorted by key, which beats the histogram setup at that size.
32-bit keys use 8-bit digits (four passes at most). 64-bit keys use 11-bit
digits (six passes at most) from `RG_ALGO_RADIX_WIDE_MIN` elements upward and
8-bit digits below that, where the per-pass bucket work of wide digits would
dominate; all three settings are configurable.

## Choosing a sort

| Requirement | Use |
| --- | --- |
| Arbitrary comparator, any C type, no allocation or caller scratch | `rg_algo_sort_name` |
| Stable arbitrary ordering with caller scratch | `rg_algo_stable_sort_name` |
| Stable 32-bit integer-key throughput with caller scratch | `rg_algo_radix_sort_name` |
| 64-bit integer keys | Measure: `rg_algo_sort_name` or `rg_algo_radix_sort_name` |

Radix sorting leads clearly for 32-bit keys, and with 11-bit digits it also
leads for full-range 64-bit keys from a few thousand elements upward (the
one-million 32-byte record workload takes 51 ms against 54 ms for the
block-partitioned comparison sort). Below that the comparison sort is faster
for 64-bit keys; prefer the radix sort when stability is required or the key
distribution leaves most digits constant.

## Performance

On the published one-million-element random `int32` workload, the typed radix
sort completed in 15.38 ms versus 89.63 ms for `std::sort` and 142.05 ms for
`qsort` (MSVC, Ryzen 9 4900HS). On a low-end Linux laptop (Intel Core i3-1005G1,
GCC 13.3) the same workload takes 8.1 ms for the radix sort, 40.7 ms for the
block-partition comparison sort, and 74.9 ms for libstdc++'s `std::sort`;
already sorted million-element input costs the radix sort 1.8 ms;
`rg_algo_nth_element` takes 2.7 ms against 5.3 ms for `std::nth_element`. See
the [core benchmark report](benchmarks/rg_core.md#rg_algo) for record sorting,
selection, methodology, comparison versions, and limitations.

## Configuration

Define options before including the header:

```c
#define RG_ALGO_ASSERT(condition)       // Custom assertion macro
#define RG_ALGO_INVALID SIZE_MAX        // Invalid index sentinel
#define RG_ALGO_INSERTION_CUTOFF 24     // Quicksort insertion cutoff
#define RG_ALGO_STABLE_RUN 8            // Stable-sort insertion run
#define RG_ALGO_STACK_CAP 64             // Quicksort range stack capacity
#define RG_ALGO_BLOCK 64                 // Block partition size
#define RG_ALGO_NINTHER_CUTOFF 128       // Median-of-medians pivot cutoff
#define RG_ALGO_RADIX_BITS 8             // Bits per radix pass, 32-bit keys
#define RG_ALGO_RADIX_BITS_U64 11        // Bits per radix pass, 64-bit keys of RG_ALGO_RADIX_WIDE_MIN+ elements
#define RG_ALGO_RADIX_WIDE_MIN 2048      // Smaller 64-bit sorts use RG_ALGO_RADIX_BITS digits
#define RG_ALGO_RADIX_INSERTION_CUTOFF 32 // Radix inputs this small are insertion sorted
#include "rg_algo.h"
```

`RG_ALGO_INSERTION_CUTOFF` and `RG_ALGO_RADIX_INSERTION_CUTOFF` must be at
least 2. `RG_ALGO_RADIX_BITS` and `RG_ALGO_RADIX_BITS_U64` may range from 4
through 12. More bits reduce the number of passes but enlarge the two
stack-allocated histogram tables, each `2^bits * sizeof(size_t)` bytes: 4 KiB
per table at the 8-bit default, 16 KiB at 11, 32 KiB at 12. If the quicksort
range stack reaches `RG_ALGO_STACK_CAP`, the range that would have been pushed
safely falls back to heap sort. `RG_ALGO_BLOCK` may range from 1 through 255
and sets how many elements each partition block scans before swapping; it also
sizes two stack-allocated offset tables of that many bytes. Ranges larger than
`RG_ALGO_NINTHER_CUTOFF` (at least 8) choose the pivot from three medians of
three instead of one.

## Complexity

- Sort: `O(n log n)` worst-case time and no caller scratch.
- Stable sort: `O(n log n)` time and `count * sizeof(type)` scratch.
- `nth_element`: linear expected time and quadratic worst-case time.
- Binary search and bounds: `O(log n)` time.
- Min/max operations: `O(n)` time.
- Radix sort: `O(n)` per varying digit plus one key scan, with `count *
  sizeof(type)` scratch; already sorted input costs the scan alone.

## Thread safety

The header has no mutable global state. Generated algorithms may run
concurrently on independent arrays. Concurrent access to the same array
requires caller synchronization.
