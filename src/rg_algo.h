// rg_algo - Typed sorting, selection, and searching for C
//
// Part of the Reverse Gravity (rg_) core libraries.
// Single-header macro-generated algorithms for caller-defined value types.
//
// USAGE:
//   #include "rg_algo.h"
//
//   static int i32_less(const i32* a, const i32* b) { return *a < *b; }
//   RG_ALGO_DEFINE(i32, I32Algo, i32_less);
//
//   i32 values[8] = {5, 1, 4, 2, 3, 0, 9, 7};
//   rg_algo_sort_I32Algo(values, 8);
//
// OPTIONS:
//   #define RG_ALGO_ASSERT(x)            - Custom assert macro (default: assert)
//   #define RG_ALGO_INVALID              - Invalid index sentinel (default: SIZE_MAX)
//   #define RG_ALGO_INSERTION_CUTOFF     - Insertion sort cutoff (default: 24)
//   #define RG_ALGO_STABLE_RUN            - Stable sort insertion-run size (default: 8)
//   #define RG_ALGO_STACK_CAP            - Quicksort range stack capacity (default: 64)
//   #define RG_ALGO_BLOCK                - Block partition size (default: 64)
//   #define RG_ALGO_NINTHER_CUTOFF       - Ninther pivot cutoff (default: 128)
//   #define RG_ALGO_RADIX_BITS           - Bits per radix pass (default: 8)
//
// NOTES:
//   - Comparators must implement strict weak ordering.
//   - Prefer macro or RGINLINE comparators for best performance.
//   - Stable sort requires a caller-provided scratch buffer.
//   - Radix sort requires integer keys and a scratch buffer.
//   - Scratch buffers must not overlap the input data.
//   - Generated functions have internal linkage and work in unity builds.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_ALGO_H
#define RG_ALGO_H

#include "rg_defs.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef RG_ALGO_ASSERT
#include <assert.h>
#define RG_ALGO_ASSERT(x) assert(x)
#endif

#ifndef RG_ALGO_INVALID
#define RG_ALGO_INVALID SIZE_MAX
#endif

#ifndef RG_ALGO_INSERTION_CUTOFF
#define RG_ALGO_INSERTION_CUTOFF 24
#endif

#if RG_ALGO_INSERTION_CUTOFF < 2
#error RG_ALGO_INSERTION_CUTOFF must be at least 2
#endif

#ifndef RG_ALGO_STABLE_RUN
#define RG_ALGO_STABLE_RUN 8
#endif

#if RG_ALGO_STABLE_RUN < 1
#error RG_ALGO_STABLE_RUN must be at least 1
#endif

#ifndef RG_ALGO_STACK_CAP
#define RG_ALGO_STACK_CAP 64
#endif

#if RG_ALGO_STACK_CAP < 1
#error RG_ALGO_STACK_CAP must be at least 1
#endif

#ifndef RG_ALGO_BLOCK
#define RG_ALGO_BLOCK 64
#endif

#if RG_ALGO_BLOCK < 1 || RG_ALGO_BLOCK > 255
#error RG_ALGO_BLOCK must be between 1 and 255
#endif

#ifndef RG_ALGO_NINTHER_CUTOFF
#define RG_ALGO_NINTHER_CUTOFF 128
#endif

#if RG_ALGO_NINTHER_CUTOFF < 8
#error RG_ALGO_NINTHER_CUTOFF must be at least 8
#endif

// Internal partition tuning. Ranges below RG_ALGO_BLOCK_CUTOFF elements use a
// branching Hoare partition. Larger ranges start branching when the initial
// scans cover at least RG_ALGO_SPARSE_SCAN elements (mostly ordered input) and
// stay branching while they average at most one swap per RG_ALGO_SPARSE_SCAN
// scanned elements; otherwise they switch to the block partition.
// An already partitioned range tries insertion sort first and gives up once
// it has moved more elements than the range holds.
#define RG_ALGO_BLOCK_CUTOFF 128u
#define RG_ALGO_SPARSE_SCAN 16u

#ifndef RG_ALGO_RADIX_BITS
#define RG_ALGO_RADIX_BITS 8
#endif

#if RG_ALGO_RADIX_BITS < 4 || RG_ALGO_RADIX_BITS > 16
#error RG_ALGO_RADIX_BITS must be between 4 and 16
#endif

#ifndef RG_ALGO_RADIX_BUCKETS
#define RG_ALGO_RADIX_BUCKETS (1u << RG_ALGO_RADIX_BITS)
#endif

#ifndef RG_ALGO_RADIX_MASK
#define RG_ALGO_RADIX_MASK (RG_ALGO_RADIX_BUCKETS - 1u)
#endif

#ifndef RG_ALGO_RADIX_PASSES_U32
#define RG_ALGO_RADIX_PASSES_U32 ((32u + RG_ALGO_RADIX_BITS - 1u) / RG_ALGO_RADIX_BITS)
#endif

#ifndef RG_ALGO_RADIX_PASSES_U64
#define RG_ALGO_RADIX_PASSES_U64 ((64u + RG_ALGO_RADIX_BITS - 1u) / RG_ALGO_RADIX_BITS)
#endif

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Define typed algorithms for a value type
 * @param type Value type
 * @param name Type name suffix (used in generated function names)
 * @param less_fn Comparator: int less(const type* a, const type* b)
 */
#define RG_ALGO_DEFINE(type, name, less_fn) \
	typedef type name##_Type; \
	RGINLINE int rg_algo_less_##name(const type* a, const type* b) \
	{ \
		return less_fn(a, b); \
	} \
	RGINLINE void rg_algo_swap_##name(type* a, type* b) \
	{ \
		type tmp = *a; \
		*a = *b; \
		*b = tmp; \
	} \
	RGINLINE size_t rg_algo_log2_##name(size_t value) \
	{ \
		size_t log = 0; \
		while (value > 1) \
		{ \
			value >>= 1u; \
			log++; \
		} \
		return log; \
	} \
	RGINLINE void rg_algo_insertion_sort_##name(type* data, size_t count) \
	{ \
		for (size_t i = 1; i < count; i++) \
		{ \
			type key = data[i]; \
			size_t j = i; \
			while (j > 0 && rg_algo_less_##name(&key, &data[j - 1])) \
			{ \
				data[j] = data[j - 1]; \
				j--; \
			} \
			data[j] = key; \
		} \
	} \
	RGINLINE void rg_algo_sift_down_##name(type* data, size_t start, size_t end) \
	{ \
		size_t root = start; \
		while (root < end / 2u) \
		{ \
			size_t child = root * 2u + 1u; \
			size_t swap_idx = root; \
			if (rg_algo_less_##name(&data[swap_idx], &data[child])) \
			{ \
				swap_idx = child; \
			} \
			if (child + 1u < end && rg_algo_less_##name(&data[swap_idx], &data[child + 1u])) \
			{ \
				swap_idx = child + 1u; \
			} \
			if (swap_idx == root) \
			{ \
				return; \
			} \
			rg_algo_swap_##name(&data[root], &data[swap_idx]); \
			root = swap_idx; \
		} \
	} \
	RGINLINE void rg_algo_heap_sort_##name(type* data, size_t count) \
	{ \
		if (count < 2) \
		{ \
			return; \
		} \
		for (size_t start = count / 2u; start > 0u; start--) \
		{ \
			rg_algo_sift_down_##name(data, start - 1u, count); \
		} \
		for (size_t end = count; end > 1u; end--) \
		{ \
			rg_algo_swap_##name(&data[0], &data[end - 1u]); \
			rg_algo_sift_down_##name(data, 0u, end - 1u); \
		} \
	} \
	RGINLINE void rg_algo_unguarded_insertion_sort_##name(type* begin, type* end) \
	{ \
		if (begin == end) \
		{ \
			return; \
		} \
		for (type* cur = begin + 1; cur < end; cur++) \
		{ \
			type* sift = cur; \
			type* prev = cur - 1; \
			if (rg_algo_less_##name(sift, prev)) \
			{ \
				type key = *sift; \
				do \
				{ \
					*sift-- = *prev--; \
				} while (rg_algo_less_##name(&key, prev)); \
				*sift = key; \
			} \
		} \
	} \
	RGINLINE int rg_algo_partial_insertion_sort_##name(type* begin, type* end) \
	{ \
		size_t limit = (size_t)(end - begin); \
		size_t moves = 0u; \
		if (begin == end) \
		{ \
			return 1; \
		} \
		for (type* cur = begin + 1; cur < end; cur++) \
		{ \
			type* sift = cur; \
			type* prev = cur - 1; \
			if (rg_algo_less_##name(sift, prev)) \
			{ \
				type key = *sift; \
				do \
				{ \
					*sift-- = *prev; \
				} while (sift != begin && rg_algo_less_##name(&key, --prev)); \
				*sift = key; \
				moves += (size_t)(cur - sift); \
				if (moves > limit) \
				{ \
					return 0; \
				} \
			} \
		} \
		return 1; \
	} \
	RGINLINE void rg_algo_sort3_##name(type* a, type* b, type* c) \
	{ \
		if (rg_algo_less_##name(b, a)) \
		{ \
			rg_algo_swap_##name(a, b); \
		} \
		if (rg_algo_less_##name(c, b)) \
		{ \
			rg_algo_swap_##name(b, c); \
		} \
		if (rg_algo_less_##name(b, a)) \
		{ \
			rg_algo_swap_##name(a, b); \
		} \
	} \
	RGINLINE void rg_algo_choose_pivot_##name(type* begin, type* end) \
	{ \
		size_t size = (size_t)(end - begin); \
		size_t half = size >> 1u; \
		if (size > RG_ALGO_NINTHER_CUTOFF) \
		{ \
			rg_algo_sort3_##name(begin, begin + half, end - 1); \
			rg_algo_sort3_##name(begin + 1, begin + (half - 1u), end - 2); \
			rg_algo_sort3_##name(begin + 2, begin + (half + 1u), end - 3); \
			rg_algo_sort3_##name(begin + (half - 1u), begin + half, begin + (half + 1u)); \
			rg_algo_swap_##name(begin, begin + half); \
		} \
		else \
		{ \
			rg_algo_sort3_##name(begin + half, begin, end - 1); \
		} \
	} \
	RGINLINE void rg_algo_break_patterns_##name(type* begin, type* pivot_pos, type* end) \
	{ \
		size_t left_size = (size_t)(pivot_pos - begin); \
		size_t right_size = (size_t)(end - (pivot_pos + 1)); \
		if (left_size >= 4u && left_size >= RG_ALGO_INSERTION_CUTOFF) \
		{ \
			size_t step = left_size / 4u; \
			rg_algo_swap_##name(begin, begin + step); \
			rg_algo_swap_##name(pivot_pos - 1, pivot_pos - step); \
			if (left_size > RG_ALGO_NINTHER_CUTOFF) \
			{ \
				rg_algo_swap_##name(begin + 1, begin + (step + 1u)); \
				rg_algo_swap_##name(begin + 2, begin + (step + 2u)); \
				rg_algo_swap_##name(pivot_pos - 2, pivot_pos - (step + 1u)); \
				rg_algo_swap_##name(pivot_pos - 3, pivot_pos - (step + 2u)); \
			} \
		} \
		if (right_size >= 4u && right_size >= RG_ALGO_INSERTION_CUTOFF) \
		{ \
			size_t step = right_size / 4u; \
			rg_algo_swap_##name(pivot_pos + 1, pivot_pos + (step + 1u)); \
			rg_algo_swap_##name(end - 1, end - step); \
			if (right_size > RG_ALGO_NINTHER_CUTOFF) \
			{ \
				rg_algo_swap_##name(pivot_pos + 2, pivot_pos + (step + 2u)); \
				rg_algo_swap_##name(pivot_pos + 3, pivot_pos + (step + 3u)); \
				rg_algo_swap_##name(end - 2, end - (step + 1u)); \
				rg_algo_swap_##name(end - 3, end - (step + 2u)); \
			} \
		} \
	} \
	RGINLINE void rg_algo_swap_offsets_##name(type* left_base, type* right_base, const unsigned char* offsets_l, const unsigned char* offsets_r, size_t num, int use_swaps) \
	{ \
		if (use_swaps) \
		{ \
			for (size_t i = 0; i < num; i++) \
			{ \
				rg_algo_swap_##name(left_base + offsets_l[i], right_base - offsets_r[i]); \
			} \
		} \
		else if (num > 0u) \
		{ \
			type* l = left_base + offsets_l[0]; \
			type* r = right_base - offsets_r[0]; \
			type tmp = *l; \
			*l = *r; \
			/* num <= RG_ALGO_BLOCK always holds; the explicit bound lets the \
			   compiler see the offsets arrays are never over-read. */ \
			for (size_t i = 1; i < num && i < (size_t)RG_ALGO_BLOCK; i++) \
			{ \
				l = left_base + offsets_l[i]; \
				*r = *l; \
				r = right_base - offsets_r[i]; \
				*l = *r; \
			} \
			*r = tmp; \
		} \
	} \
	RGINLINE type* rg_algo_partition_right_##name(type* begin, type* end, int* already_partitioned) \
	{ \
		type pivot = *begin; \
		type* first = begin; \
		type* last = end; \
		type* pivot_pos; \
		while (rg_algo_less_##name(++first, &pivot)) \
		{ \
		} \
		if (first - 1 == begin) \
		{ \
			while (first < last && !rg_algo_less_##name(--last, &pivot)) \
			{ \
			} \
		} \
		else \
		{ \
			while (!rg_algo_less_##name(--last, &pivot)) \
			{ \
			} \
		} \
		*already_partitioned = first >= last; \
		if (first < last && (size_t)(end - begin) < RG_ALGO_BLOCK_CUTOFF) \
		{ \
			do \
			{ \
				rg_algo_swap_##name(first, last); \
				while (rg_algo_less_##name(++first, &pivot)) \
				{ \
				} \
				while (!rg_algo_less_##name(--last, &pivot)) \
				{ \
				} \
			} while (first < last); \
		} \
		else if (first < last && (size_t)(first - begin) + (size_t)(end - last) >= RG_ALGO_SPARSE_SCAN) \
		{ \
			size_t swaps = 0u; \
			do \
			{ \
				rg_algo_swap_##name(first, last); \
				while (rg_algo_less_##name(++first, &pivot)) \
				{ \
				} \
				while (!rg_algo_less_##name(--last, &pivot)) \
				{ \
				} \
				swaps++; \
			} while (first < last && swaps * RG_ALGO_SPARSE_SCAN <= (size_t)(first - begin) + (size_t)(end - last)); \
		} \
		if (first < last) \
		{ \
			unsigned char offsets_l[RG_ALGO_BLOCK]; \
			unsigned char offsets_r[RG_ALGO_BLOCK]; \
			type* offsets_l_base; \
			type* offsets_r_base; \
			size_t num_l = 0u; \
			size_t num_r = 0u; \
			size_t start_l = 0u; \
			size_t start_r = 0u; \
			rg_algo_swap_##name(first, last); \
			++first; \
			offsets_l_base = first; \
			offsets_r_base = last; \
			while (first < last) \
			{ \
				size_t num_unknown = (size_t)(last - first); \
				size_t left_split = num_l == 0u ? (num_r == 0u ? num_unknown / 2u : num_unknown) : 0u; \
				size_t right_split = num_r == 0u ? (num_unknown - left_split) : 0u; \
				size_t num; \
				if (left_split >= RG_ALGO_BLOCK) \
				{ \
					left_split = RG_ALGO_BLOCK; \
				} \
				for (size_t i = 0; i < left_split;) \
				{ \
					offsets_l[num_l] = (unsigned char)i++; \
					num_l += (size_t)(rg_algo_less_##name(first, &pivot) == 0); \
					++first; \
				} \
				if (right_split >= RG_ALGO_BLOCK) \
				{ \
					right_split = RG_ALGO_BLOCK; \
				} \
				for (size_t i = 0; i < right_split;) \
				{ \
					offsets_r[num_r] = (unsigned char)++i; \
					num_r += (size_t)(rg_algo_less_##name(--last, &pivot) != 0); \
				} \
				num = num_l < num_r ? num_l : num_r; \
				rg_algo_swap_offsets_##name(offsets_l_base, offsets_r_base, offsets_l + start_l, offsets_r + start_r, num, num_l == num_r); \
				num_l -= num; \
				num_r -= num; \
				start_l += num; \
				start_r += num; \
				if (num_l == 0u) \
				{ \
					start_l = 0u; \
					offsets_l_base = first; \
				} \
				if (num_r == 0u) \
				{ \
					start_r = 0u; \
					offsets_r_base = last; \
				} \
			} \
			if (num_l > 0u) \
			{ \
				const unsigned char* offsets = offsets_l + start_l; \
				while (num_l-- > 0u) \
				{ \
					rg_algo_swap_##name(offsets_l_base + offsets[num_l], --last); \
				} \
				first = last; \
			} \
			if (num_r > 0u) \
			{ \
				const unsigned char* offsets = offsets_r + start_r; \
				while (num_r-- > 0u) \
				{ \
					rg_algo_swap_##name(first, offsets_r_base - offsets[num_r]); \
					++first; \
				} \
				last = first; \
			} \
		} \
		pivot_pos = first - 1; \
		*begin = *pivot_pos; \
		*pivot_pos = pivot; \
		return pivot_pos; \
	} \
	RGINLINE type* rg_algo_partition_left_##name(type* begin, type* end) \
	{ \
		type pivot = *begin; \
		type* first = begin; \
		type* last = end; \
		type* pivot_pos; \
		while (rg_algo_less_##name(&pivot, --last)) \
		{ \
		} \
		if (last + 1 == end) \
		{ \
			while (first < last && !rg_algo_less_##name(&pivot, ++first)) \
			{ \
			} \
		} \
		else \
		{ \
			while (!rg_algo_less_##name(&pivot, ++first)) \
			{ \
			} \
		} \
		while (first < last) \
		{ \
			rg_algo_swap_##name(first, last); \
			while (rg_algo_less_##name(&pivot, --last)) \
			{ \
			} \
			while (!rg_algo_less_##name(&pivot, ++first)) \
			{ \
			} \
		} \
		pivot_pos = last; \
		*begin = *pivot_pos; \
		*pivot_pos = pivot; \
		return pivot_pos; \
	} \
	typedef struct rg_algo_range_##name \
	{ \
		type* begin; \
		type* end; \
		size_t bad_allowed; \
		int leftmost; \
	} rg_algo_range_##name; \
	RGINLINE void rg_algo_push_##name(rg_algo_range_##name* stack, size_t* top, type* begin, type* end, size_t bad_allowed, int leftmost) \
	{ \
		rg_algo_range_##name range; \
		range.begin = begin; \
		range.end = end; \
		range.bad_allowed = bad_allowed; \
		range.leftmost = leftmost; \
		stack[(*top)++] = range; \
	} \
	RGINLINE void rg_algo_sort_##name(type* data, size_t count) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		if (count < 2) \
		{ \
			return; \
		} \
		size_t different = 1u; \
		while (different < count && \
			   !rg_algo_less_##name(&data[different - 1u], &data[different]) && \
			   !rg_algo_less_##name(&data[different], &data[different - 1u])) \
		{ \
			different++; \
		} \
		if (different == count) \
		{ \
			return; \
		} \
		size_t ordered = different + 1u; \
		if (rg_algo_less_##name(&data[different], &data[different - 1u])) \
		{ \
			while (ordered < count && \
				   !rg_algo_less_##name(&data[ordered - 1u], &data[ordered])) \
			{ \
				ordered++; \
			} \
			if (ordered == count) \
			{ \
				size_t left = 0u; \
				size_t right = count - 1u; \
				while (left < right) \
				{ \
					rg_algo_swap_##name(&data[left], &data[right]); \
					left++; \
					right--; \
				} \
				return; \
			} \
		} \
		else \
		{ \
			while (ordered < count && \
				   !rg_algo_less_##name(&data[ordered], &data[ordered - 1u])) \
			{ \
				ordered++; \
			} \
			if (ordered == count) \
			{ \
				return; \
			} \
		} \
		rg_algo_range_##name stack[RG_ALGO_STACK_CAP]; \
		size_t top = 0u; \
		rg_algo_push_##name(stack, &top, data, data + count, rg_algo_log2_##name(count), 1); \
		while (top > 0u) \
		{ \
			rg_algo_range_##name range = stack[--top]; \
			type* begin = range.begin; \
			type* end = range.end; \
			size_t bad_allowed = range.bad_allowed; \
			int leftmost = range.leftmost; \
			for (;;) \
			{ \
				size_t size = (size_t)(end - begin); \
				size_t left_size; \
				size_t right_size; \
				type* pivot_pos; \
				int already_partitioned; \
				if (size <= RG_ALGO_INSERTION_CUTOFF) \
				{ \
					if (leftmost) \
					{ \
						rg_algo_insertion_sort_##name(begin, size); \
					} \
					else \
					{ \
						rg_algo_unguarded_insertion_sort_##name(begin, end); \
					} \
					break; \
				} \
				rg_algo_choose_pivot_##name(begin, end); \
				if (!leftmost && !rg_algo_less_##name(begin - 1, begin)) \
				{ \
					begin = rg_algo_partition_left_##name(begin, end) + 1; \
					continue; \
				} \
				pivot_pos = rg_algo_partition_right_##name(begin, end, &already_partitioned); \
				left_size = (size_t)(pivot_pos - begin); \
				right_size = (size_t)(end - (pivot_pos + 1)); \
				if (left_size < size / 8u || right_size < size / 8u) \
				{ \
					if (--bad_allowed == 0u) \
					{ \
						rg_algo_heap_sort_##name(begin, size); \
						break; \
					} \
					rg_algo_break_patterns_##name(begin, pivot_pos, end); \
				} \
				else if (already_partitioned && \
				         rg_algo_partial_insertion_sort_##name(begin, pivot_pos) && \
				         rg_algo_partial_insertion_sort_##name(pivot_pos + 1, end)) \
				{ \
					break; \
				} \
				if (left_size < right_size) \
				{ \
					if (right_size > 1u) \
					{ \
						if (top >= RG_ALGO_STACK_CAP) \
						{ \
							rg_algo_heap_sort_##name(pivot_pos + 1, right_size); \
						} \
						else \
						{ \
							rg_algo_push_##name(stack, &top, pivot_pos + 1, end, bad_allowed, 0); \
						} \
					} \
					end = pivot_pos; \
				} \
				else \
				{ \
					if (left_size > 1u) \
					{ \
						if (top >= RG_ALGO_STACK_CAP) \
						{ \
							rg_algo_heap_sort_##name(begin, left_size); \
						} \
						else \
						{ \
							rg_algo_push_##name(stack, &top, begin, pivot_pos, bad_allowed, leftmost); \
						} \
					} \
					begin = pivot_pos + 1; \
					leftmost = 0; \
				} \
			} \
		} \
	} \
	RGINLINE void rg_algo_merge_##name(const type* src, type* dst, size_t left, size_t mid, size_t right) \
	{ \
		size_t i = left; \
		size_t j = mid; \
		size_t k = left; \
		while (i < mid && j < right) \
		{ \
			if (rg_algo_less_##name(&src[j], &src[i])) \
			{ \
				dst[k++] = src[j++]; \
			} \
			else \
			{ \
				dst[k++] = src[i++]; \
			} \
		} \
		if (i < mid) \
		{ \
			memcpy(dst + k, src + i, (mid - i) * sizeof(type)); \
		} \
		else if (j < right) \
		{ \
			memcpy(dst + k, src + j, (right - j) * sizeof(type)); \
		} \
	} \
	RGINLINE void rg_algo_stable_sort_##name(type* data, size_t count, type* scratch, size_t scratch_count) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		if (count < 2) \
		{ \
			return; \
		} \
		RG_ALGO_ASSERT(scratch != NULL); \
		RG_ALGO_ASSERT(scratch_count >= count); \
		RG_ALGO_ASSERT(count <= SIZE_MAX / sizeof(type)); \
		size_t byte_count = count * sizeof(type); \
		uintptr_t data_address = (uintptr_t)(const void*)data; \
		uintptr_t scratch_address = (uintptr_t)(const void*)scratch; \
		RG_ALGO_ASSERT((scratch_address < data_address && data_address - scratch_address >= byte_count) || \
		               (scratch_address > data_address && scratch_address - data_address >= byte_count)); \
		size_t sorted_count = 1u; \
		while (sorted_count < count && !rg_algo_less_##name(&data[sorted_count], &data[sorted_count - 1u])) \
		{ \
			sorted_count++; \
		} \
		if (sorted_count == count) \
		{ \
			return; \
		} \
		size_t run = 0u; \
		while (run < count) \
		{ \
			size_t run_count = count - run; \
			if (run_count > RG_ALGO_STABLE_RUN) \
			{ \
				run_count = RG_ALGO_STABLE_RUN; \
			} \
			rg_algo_insertion_sort_##name(data + run, run_count); \
			run += run_count; \
		} \
		if (count <= RG_ALGO_STABLE_RUN) \
		{ \
			return; \
		} \
		type* src = data; \
		type* dst = scratch; \
		size_t width = RG_ALGO_STABLE_RUN; \
		while (width < count) \
		{ \
			size_t i = 0u; \
			while (i < count) \
			{ \
				size_t left = i; \
				size_t mid = count - i > width ? i + width : count; \
				size_t right = count - mid > width ? mid + width : count; \
				if (mid >= right) \
				{ \
					memcpy(dst + left, src + left, (right - left) * sizeof(type)); \
				} \
				else if (!rg_algo_less_##name(&src[mid], &src[mid - 1u])) \
				{ \
					memcpy(dst + left, src + left, (right - left) * sizeof(type)); \
				} \
				else \
				{ \
					rg_algo_merge_##name(src, dst, left, mid, right); \
				} \
				i = right; \
			} \
			type* tmp = src; \
			src = dst; \
			dst = tmp; \
			if (width > count / 2u) \
			{ \
				break; \
			} \
			width <<= 1u; \
		} \
		if (src != data) \
		{ \
			memcpy(data, src, count * sizeof(type)); \
		} \
	} \
	RGINLINE void rg_algo_nth_element_##name(type* data, size_t count, size_t nth) \
	{ \
		RG_ALGO_ASSERT(data != NULL); \
		RG_ALGO_ASSERT(nth < count); \
		type* begin = data; \
		type* end = data + count; \
		type* target = data + nth; \
		int leftmost = 1; \
		while ((size_t)(end - begin) > RG_ALGO_INSERTION_CUTOFF) \
		{ \
			size_t size = (size_t)(end - begin); \
			type* pivot_pos; \
			int already_partitioned; \
			rg_algo_choose_pivot_##name(begin, end); \
			if (!leftmost && !rg_algo_less_##name(begin - 1, begin)) \
			{ \
				pivot_pos = rg_algo_partition_left_##name(begin, end); \
				if (target <= pivot_pos) \
				{ \
					return; \
				} \
				begin = pivot_pos + 1; \
				continue; \
			} \
			pivot_pos = rg_algo_partition_right_##name(begin, end, &already_partitioned); \
			if (target == pivot_pos) \
			{ \
				return; \
			} \
			if ((size_t)(pivot_pos - begin) < size / 8u || (size_t)(end - (pivot_pos + 1)) < size / 8u) \
			{ \
				rg_algo_break_patterns_##name(begin, pivot_pos, end); \
			} \
			if (target < pivot_pos) \
			{ \
				end = pivot_pos; \
			} \
			else \
			{ \
				begin = pivot_pos + 1; \
				leftmost = 0; \
			} \
		} \
		if (leftmost) \
		{ \
			rg_algo_insertion_sort_##name(begin, (size_t)(end - begin)); \
		} \
		else \
		{ \
			rg_algo_unguarded_insertion_sort_##name(begin, end); \
		} \
	} \
	RGINLINE int rg_algo_is_sorted_##name(const type* data, size_t count) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		for (size_t i = 1; i < count; i++) \
		{ \
			if (rg_algo_less_##name(&data[i], &data[i - 1u])) \
			{ \
				return 0; \
			} \
		} \
		return 1; \
	} \
	RGINLINE size_t rg_algo_lower_bound_##name(const type* data, size_t count, const type* key) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		RG_ALGO_ASSERT(key != NULL); \
		size_t first = 0u; \
		size_t len = count; \
		while (len > 0u) \
		{ \
			size_t half = len >> 1u; \
			size_t mid = first + half; \
			if (rg_algo_less_##name(&data[mid], key)) \
			{ \
				first = mid + 1u; \
				len -= half + 1u; \
			} \
			else \
			{ \
				len = half; \
			} \
		} \
		return first; \
	} \
	RGINLINE size_t rg_algo_upper_bound_##name(const type* data, size_t count, const type* key) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		RG_ALGO_ASSERT(key != NULL); \
		size_t first = 0u; \
		size_t len = count; \
		while (len > 0u) \
		{ \
			size_t half = len >> 1u; \
			size_t mid = first + half; \
			if (rg_algo_less_##name(key, &data[mid])) \
			{ \
				len = half; \
			} \
			else \
			{ \
				first = mid + 1u; \
				len -= half + 1u; \
			} \
		} \
		return first; \
	} \
	RGINLINE size_t rg_algo_binary_search_##name(const type* data, size_t count, const type* key) \
	{ \
		size_t idx = rg_algo_lower_bound_##name(data, count, key); \
		if (idx < count && !rg_algo_less_##name(key, &data[idx])) \
		{ \
			return idx; \
		} \
		return RG_ALGO_INVALID; \
	} \
	RGINLINE size_t rg_algo_min_index_##name(const type* data, size_t count) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		if (count == 0u) \
		{ \
			return RG_ALGO_INVALID; \
		} \
		size_t idx = 0u; \
		for (size_t i = 1; i < count; i++) \
		{ \
			if (rg_algo_less_##name(&data[i], &data[idx])) \
			{ \
				idx = i; \
			} \
		} \
		return idx; \
	} \
	RGINLINE size_t rg_algo_max_index_##name(const type* data, size_t count) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		if (count == 0u) \
		{ \
			return RG_ALGO_INVALID; \
		} \
		size_t idx = 0u; \
		for (size_t i = 1; i < count; i++) \
		{ \
			if (rg_algo_less_##name(&data[idx], &data[i])) \
			{ \
				idx = i; \
			} \
		} \
		return idx; \
	} \
	RGINLINE void rg_algo_minmax_index_##name(const type* data, size_t count, size_t* out_min_idx, size_t* out_max_idx) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		if (count == 0u) \
		{ \
			if (out_min_idx != NULL) \
			{ \
				*out_min_idx = RG_ALGO_INVALID; \
			} \
			if (out_max_idx != NULL) \
			{ \
				*out_max_idx = RG_ALGO_INVALID; \
			} \
			return; \
		} \
		size_t min_idx = 0u; \
		size_t max_idx = 0u; \
		size_t i = 1u; \
		if (count > 1u) \
		{ \
			if (rg_algo_less_##name(&data[1u], &data[0u])) \
			{ \
				min_idx = 1u; \
				max_idx = 0u; \
			} \
			else \
			{ \
				min_idx = 0u; \
				max_idx = 1u; \
			} \
			i = 2u; \
		} \
		for (; count - i > 1u; i += 2u) \
		{ \
			size_t lo = i; \
			size_t hi = i + 1u; \
			if (rg_algo_less_##name(&data[hi], &data[lo])) \
			{ \
				size_t tmp = lo; \
				lo = hi; \
				hi = tmp; \
			} \
			if (rg_algo_less_##name(&data[lo], &data[min_idx])) \
			{ \
				min_idx = lo; \
			} \
			if (rg_algo_less_##name(&data[max_idx], &data[hi])) \
			{ \
				max_idx = hi; \
			} \
		} \
		if (i < count) \
		{ \
			if (rg_algo_less_##name(&data[i], &data[min_idx])) \
			{ \
				min_idx = i; \
			} \
			if (rg_algo_less_##name(&data[max_idx], &data[i])) \
			{ \
				max_idx = i; \
			} \
		} \
		if (out_min_idx != NULL) \
		{ \
			*out_min_idx = min_idx; \
		} \
		if (out_max_idx != NULL) \
		{ \
			*out_max_idx = max_idx; \
		} \
	}

/**
 * @brief Define typed radix sort for 32-bit integer keys
 * @param type Value type
 * @param name Type name suffix (used in generated function names)
 * @param key_fn Key extractor: u32 key(const type* value)
 */
#define RG_ALGO_RADIX_U32_DEFINE(type, name, key_fn) \
	typedef type name##_Type; \
	RGINLINE u32 rg_algo_key_##name(const type* value) \
	{ \
		return (u32)(key_fn(value)); \
	} \
	RGINLINE void rg_algo_radix_sort_##name(type* data, size_t count, type* scratch, size_t scratch_count) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		if (count < 2u) \
		{ \
			return; \
		} \
		RG_ALGO_ASSERT(scratch != NULL); \
		RG_ALGO_ASSERT(scratch_count >= count); \
		RG_ALGO_ASSERT(count <= SIZE_MAX / sizeof(type)); \
		size_t byte_count = count * sizeof(type); \
		uintptr_t data_address = (uintptr_t)(const void*)data; \
		uintptr_t scratch_address = (uintptr_t)(const void*)scratch; \
		RG_ALGO_ASSERT((scratch_address < data_address && data_address - scratch_address >= byte_count) || \
		               (scratch_address > data_address && scratch_address - data_address >= byte_count)); \
		type* src = data; \
		type* dst = scratch; \
		size_t counts[RG_ALGO_RADIX_BUCKETS]; \
		u32 key_base = rg_algo_key_##name(&data[0]); \
		u32 varying = 0u; \
		for (size_t pass = 0; pass < RG_ALGO_RADIX_PASSES_U32; pass++) \
		{ \
			u32 shift = (u32)(pass * RG_ALGO_RADIX_BITS); \
			if (pass > 0u && ((varying >> shift) & (u32)RG_ALGO_RADIX_MASK) == 0u) \
			{ \
				continue; \
			} \
			memset(counts, 0, sizeof(counts)); \
			for (size_t i = 0; i < count; i++) \
			{ \
				u32 key = rg_algo_key_##name(&src[i]); \
				if (pass == 0u) \
				{ \
					varying |= key ^ key_base; \
				} \
				size_t bucket = (size_t)((key >> shift) & (u32)RG_ALGO_RADIX_MASK); \
				counts[bucket]++; \
			} \
			size_t sum = 0; \
			size_t occupied = 0; \
			for (size_t b = 0; b < RG_ALGO_RADIX_BUCKETS; b++) \
			{ \
				size_t c = counts[b]; \
				counts[b] = sum; \
				sum += c; \
				occupied += (c != 0u); \
			} \
			if (occupied < 2u) \
			{ \
				continue; \
			} \
			for (size_t i = 0; i < count; i++) \
			{ \
				u32 key = rg_algo_key_##name(&src[i]); \
				size_t bucket = (size_t)((key >> shift) & (u32)RG_ALGO_RADIX_MASK); \
				dst[counts[bucket]++] = src[i]; \
			} \
			type* tmp = src; \
			src = dst; \
			dst = tmp; \
		} \
		if (src != data) \
		{ \
			memcpy(data, src, count * sizeof(type)); \
		} \
	}

/**
 * @brief Define typed radix sort for 64-bit integer keys
 * @param type Value type
 * @param name Type name suffix (used in generated function names)
 * @param key_fn Key extractor: u64 key(const type* value)
 */
#define RG_ALGO_RADIX_U64_DEFINE(type, name, key_fn) \
	typedef type name##_Type; \
	RGINLINE u64 rg_algo_key_##name(const type* value) \
	{ \
		return (u64)(key_fn(value)); \
	} \
	RGINLINE void rg_algo_radix_sort_##name(type* data, size_t count, type* scratch, size_t scratch_count) \
	{ \
		RG_ALGO_ASSERT(data != NULL || count == 0u); \
		if (count < 2u) \
		{ \
			return; \
		} \
		RG_ALGO_ASSERT(scratch != NULL); \
		RG_ALGO_ASSERT(scratch_count >= count); \
		RG_ALGO_ASSERT(count <= SIZE_MAX / sizeof(type)); \
		size_t byte_count = count * sizeof(type); \
		uintptr_t data_address = (uintptr_t)(const void*)data; \
		uintptr_t scratch_address = (uintptr_t)(const void*)scratch; \
		RG_ALGO_ASSERT((scratch_address < data_address && data_address - scratch_address >= byte_count) || \
		               (scratch_address > data_address && scratch_address - data_address >= byte_count)); \
		type* src = data; \
		type* dst = scratch; \
		size_t counts[RG_ALGO_RADIX_BUCKETS]; \
		u64 key_base = rg_algo_key_##name(&data[0]); \
		u64 varying = 0u; \
		for (size_t pass = 0; pass < RG_ALGO_RADIX_PASSES_U64; pass++) \
		{ \
			u32 shift = (u32)(pass * RG_ALGO_RADIX_BITS); \
			if (pass > 0u && ((varying >> shift) & (u64)RG_ALGO_RADIX_MASK) == 0u) \
			{ \
				continue; \
			} \
			memset(counts, 0, sizeof(counts)); \
			for (size_t i = 0; i < count; i++) \
			{ \
				u64 key = rg_algo_key_##name(&src[i]); \
				if (pass == 0u) \
				{ \
					varying |= key ^ key_base; \
				} \
				size_t bucket = (size_t)((key >> shift) & (u64)RG_ALGO_RADIX_MASK); \
				counts[bucket]++; \
			} \
			size_t sum = 0; \
			size_t occupied = 0; \
			for (size_t b = 0; b < RG_ALGO_RADIX_BUCKETS; b++) \
			{ \
				size_t c = counts[b]; \
				counts[b] = sum; \
				sum += c; \
				occupied += (c != 0u); \
			} \
			if (occupied < 2u) \
			{ \
				continue; \
			} \
			for (size_t i = 0; i < count; i++) \
			{ \
				u64 key = rg_algo_key_##name(&src[i]); \
				size_t bucket = (size_t)((key >> shift) & (u64)RG_ALGO_RADIX_MASK); \
				dst[counts[bucket]++] = src[i]; \
			} \
			type* tmp = src; \
			src = dst; \
			dst = tmp; \
		} \
		if (src != data) \
		{ \
			memcpy(data, src, count * sizeof(type)); \
		} \
	}

#endif // RG_ALGO_H
