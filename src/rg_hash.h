// rg_hash - Hash functions and typed open-addressing hash tables for C
//
// Part of the Reverse Gravity (rg_) core libraries.
// Single-header hashes plus arena-backed, macro-generated hash maps and sets.
// Tables keep a one-byte control tag per slot and probe them a group at a time
// with SSE2, NEON, or 64-bit arithmetic, so a lookup touches entries only when
// a tag matches.
//
// USAGE:
//   #include "rg_hash.h"
//
//   RG_HASH_MAP_DEFINE(uint32_t, float, U32FloatMap, rg_hash_u32_fast, rg_hash_eq_u32);
//   U32FloatMap map;
//   rg_hash_map_init(U32FloatMap, &map, &arena);
//   rg_hash_map_put(U32FloatMap, &map, 42, 3.14f, NULL);
//
// OPTIONS:
//   #define RG_HASH_ASSERT(x)       - Custom assert macro
//   #define RG_HASH_MIN_CAP         - Minimum table capacity (default: 8)
//   #define RG_HASH_MAX_LOAD_NUM    - Load factor numerator (default: 7)
//   #define RG_HASH_MAX_LOAD_DEN    - Load factor denominator (default: 10)
//   #define RG_HASH_SEED            - Default hash seed (default: 0)
//   #define RG_HASH_NO_SIMD         - Probe control bytes with 64-bit arithmetic instead of SSE2/NEON
//
// NOTES:
//   - Hash tables are arena-backed; growth (and the occasional same-size rebuild
//     that drops tombstones) leaves old storage in the arena.
//   - entries[i].hash == 0 marks an unoccupied slot; hashes are forced non-zero.
//     Control bytes decide probing; entry hashes are for iteration and growth.
//   - Maps and sets are not safe for concurrent mutation.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_HASH_H
#define RG_HASH_H

#include "rg_mem.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !defined(RG_HASH_FORCE_PORTABLE_MUL128)
#if RG_COMPILER_MSVC && RG_ARCH_X64
#include <intrin.h>
#define RG_HASH_USE_UMUL128 1
#elif defined(__SIZEOF_INT128__)
#define RG_HASH_USE_INT128 1
#endif
#endif

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef RG_HASH_ASSERT
#include <assert.h>
#define RG_HASH_ASSERT(condition) assert(condition)
#endif

// Smallest table: one full probe group so every group load stays in bounds.
#ifndef RG_HASH_MIN_CAP
#define RG_HASH_MIN_CAP 16
#endif

#ifndef RG_HASH_MAX_LOAD_NUM
#define RG_HASH_MAX_LOAD_NUM 7
#endif

#ifndef RG_HASH_MAX_LOAD_DEN
#define RG_HASH_MAX_LOAD_DEN 10
#endif

#ifndef RG_HASH_SEED
#define RG_HASH_SEED 0
#endif

#ifndef RG_HASH_INVALID
#define RG_HASH_INVALID SIZE_MAX
#endif

#if RG_HASH_MIN_CAP < 16 || (RG_HASH_MIN_CAP & (RG_HASH_MIN_CAP - 1)) != 0
#error RG_HASH_MIN_CAP must be a power of two of at least 16
#endif

#if RG_HASH_MAX_LOAD_NUM == 0 || RG_HASH_MAX_LOAD_DEN == 0 || \
    RG_HASH_MAX_LOAD_NUM >= RG_HASH_MAX_LOAD_DEN
#error RG_HASH_MAX_LOAD_NUM / RG_HASH_MAX_LOAD_DEN must be between zero and one
#endif

// Table growth is the cold path of put/insert: keep it out of line so every
// call site inlines only the probe loop. The macro stays defined because the
// table-defining macros expand in user translation units.
#if RG_COMPILER_MSVC
#define RG_HASH_COLD static __declspec(noinline) __inline
#elif RG_COMPILER_GCC || RG_COMPILER_CLANG
#define RG_HASH_COLD static __attribute__((noinline, unused))
#else
#define RG_HASH_COLD static
#endif

// =============================================================================
// PUBLIC API - Hash Functions
// =============================================================================

/**
 * @brief Hash a byte buffer (xxhash64)
 * @param data Pointer to bytes
 * @param len Number of bytes
 * @param seed Hash seed
 * @return 64-bit hash
 */
RGINLINE uint64_t rg_hash_bytes(const void* data, size_t len, uint64_t seed);

/**
 * @brief Hash a null-terminated string using RG_HASH_SEED
 * @param str String to hash
 * @return 64-bit hash
 */
RGINLINE uint64_t rg_hash_str(const char* str);

/**
 * @brief Hash a 32-bit integer using a fast mix
 * @param value Input value
 * @return 64-bit hash
 */
RGINLINE uint64_t rg_hash_u32(uint32_t value);

/**
 * @brief Hash a 64-bit integer using a fast mix
 * @param value Input value
 * @return 64-bit hash
 */
RGINLINE uint64_t rg_hash_u64(uint64_t value);

/**
 * @brief Hash a pointer value
 * @param ptr Pointer to hash
 * @return 64-bit hash
 */
RGINLINE uint64_t rg_hash_ptr(const void* ptr);

/**
 * @brief Hash a 32-bit integer for a power-of-two hash table
 * @details One multiply; the result's low bits are well mixed, which is what
 *          the table's mask consumes. Cheaper than rg_hash_u32 and intended as
 *          the hash function for integer-keyed maps and sets. Not stable for
 *          persistent identifiers: use rg_hash_u32 for values that are stored.
 * @param value Input value
 * @return 64-bit hash
 */
RGINLINE uint64_t rg_hash_u32_fast(uint32_t value);

/**
 * @brief Hash a 64-bit integer for a power-of-two hash table
 * @details The high and low halves of the full 128-bit product with an odd
 *          constant, XORed together: one multiply instruction on x86-64 and
 *          AArch64. Every input bit reaches every output bit, so keys that
 *          differ only in their upper bits (tagged values, packed pairs,
 *          aligned pointers) spread as well as random keys. See
 *          rg_hash_u32_fast for the intended use.
 * @param value Input value
 * @return 64-bit hash
 */
RGINLINE uint64_t rg_hash_u64_fast(uint64_t value);

/**
 * @brief Hash a pointer for a power-of-two hash table; see rg_hash_u64_fast
 * @param ptr Pointer to hash
 * @return 64-bit hash
 */
RGINLINE uint64_t rg_hash_ptr_fast(const void* ptr);

/**
 * @brief Equality helpers for common key types
 */
RGINLINE int rg_hash_eq_u32(uint32_t a, uint32_t b);
RGINLINE int rg_hash_eq_u64(uint64_t a, uint64_t b);
RGINLINE int rg_hash_eq_ptr(const void* a, const void* b);
RGINLINE int rg_hash_eq_str(const char* a, const char* b);

// =============================================================================
// PUBLIC API - Hash Tables
// =============================================================================

// =============================================================================
// TABLE MACHINERY - control bytes and group probing
// =============================================================================
//
// Every slot has a control byte: RG_HASH_CTRL_EMPTY, RG_HASH_CTRL_DELETED, or
// the key's seven-bit tag (hash bits 25..31, which the recommended hash functions
// spread well and which lie above the index bits of any table below 2^25
// slots). Lookups compare a whole group of control bytes against the tag with
// one SIMD instruction and read an entry only on a tag match; a group holding an
// empty byte ends an unsuccessful search. Groups step through the table with
// triangular numbers, which visit every group of a power-of-two table. The
// control array carries RG_HASH_GROUP_WIDTH mirror bytes past the end so group
// loads never wrap.

#define RG_HASH_CTRL_EMPTY 0x80u
#define RG_HASH_CTRL_DELETED 0xFEu

RGINLINE uint64_t rg_hash_bswap64(uint64_t value);

#if !defined(RG_HASH_NO_SIMD) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#include <emmintrin.h>
#define RG_HASH_GROUP_WIDTH 16
#define RG_HASH_GROUP_SSE2 1
#elif !defined(RG_HASH_NO_SIMD) && defined(__ARM_NEON) && (defined(__GNUC__) || defined(__clang__))
#include <arm_neon.h>
#define RG_HASH_GROUP_WIDTH 8
#define RG_HASH_GROUP_NEON 1
#else
#define RG_HASH_GROUP_WIDTH 8
#define RG_HASH_GROUP_SWAR 1
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

RGINLINE unsigned rg_hash_ctz64(uint64_t value)
{
#if defined(__GNUC__) || defined(__clang__)
	return (unsigned)__builtin_ctzll(value);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
	unsigned long index;
	_BitScanForward64(&index, value);
	return (unsigned)index;
#else
	unsigned n = 0;
	while ((value & 1u) == 0)
	{
		value >>= 1;
		n++;
	}
	return n;
#endif
}

RGINLINE unsigned rg_hash_clz64(uint64_t value)
{
#if defined(__GNUC__) || defined(__clang__)
	return (unsigned)__builtin_clzll(value);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
	unsigned long index;
	_BitScanReverse64(&index, value);
	return 63u - (unsigned)index;
#else
	unsigned n = 0;
	while ((value >> 63) == 0)
	{
		value <<= 1;
		n++;
	}
	return n;
#endif
}

// A group of control bytes and the bit masks the comparisons produce. In the
// SSE2 backend a mask has one bit per lane; in the NEON and portable backends
// each lane owns the top bit of its byte, so lane index = bit index / 8.
#if defined(RG_HASH_GROUP_SSE2)
typedef __m128i rg_hash_group;
typedef uint32_t rg_hash_mask;
#define RG_HASH_MASK_LANE_SHIFT 0
RGINLINE rg_hash_group rg_hash_group_load(const uint8_t* ctrl) { return _mm_loadu_si128((const __m128i*)ctrl); }
RGINLINE rg_hash_mask rg_hash_group_match(rg_hash_group g, uint8_t tag)
{
	return (rg_hash_mask)_mm_movemask_epi8(_mm_cmpeq_epi8(g, _mm_set1_epi8((char)tag)));
}
RGINLINE rg_hash_mask rg_hash_group_match_empty(rg_hash_group g)
{
	return (rg_hash_mask)_mm_movemask_epi8(_mm_cmpeq_epi8(g, _mm_set1_epi8((char)RG_HASH_CTRL_EMPTY)));
}
RGINLINE rg_hash_mask rg_hash_group_match_free(rg_hash_group g) { return (rg_hash_mask)_mm_movemask_epi8(g); }
RGINLINE unsigned rg_hash_mask_leading_zeros(rg_hash_mask m) { return rg_hash_clz64(m) - 48u; }
#elif defined(RG_HASH_GROUP_NEON)
typedef uint8x8_t rg_hash_group;
typedef uint64_t rg_hash_mask;
#define RG_HASH_MASK_LANE_SHIFT 3
RGINLINE rg_hash_group rg_hash_group_load(const uint8_t* ctrl) { return vld1_u8(ctrl); }
RGINLINE rg_hash_mask rg_hash_group_match(rg_hash_group g, uint8_t tag)
{
	return vget_lane_u64(vreinterpret_u64_u8(vceq_u8(g, vdup_n_u8(tag))), 0) & UINT64_C(0x8080808080808080);
}
RGINLINE rg_hash_mask rg_hash_group_match_empty(rg_hash_group g)
{
	return vget_lane_u64(vreinterpret_u64_u8(vceq_u8(g, vdup_n_u8((uint8_t)RG_HASH_CTRL_EMPTY))), 0) & UINT64_C(0x8080808080808080);
}
RGINLINE rg_hash_mask rg_hash_group_match_free(rg_hash_group g)
{
	return vget_lane_u64(vreinterpret_u64_u8(g), 0) & UINT64_C(0x8080808080808080);
}
RGINLINE unsigned rg_hash_mask_leading_zeros(rg_hash_mask m) { return rg_hash_clz64(m) >> 3; }
#else
typedef uint64_t rg_hash_group;
typedef uint64_t rg_hash_mask;
#define RG_HASH_MASK_LANE_SHIFT 3
RGINLINE rg_hash_group rg_hash_group_load(const uint8_t* ctrl)
{
	uint64_t v;
	memcpy(&v, ctrl, sizeof(v));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	v = rg_hash_bswap64(v);
#endif
	return v;
}
// Zero-byte detection. It can report a false match in the byte after a true
// zero, which the entry comparison rejects; it never misses a match.
RGINLINE rg_hash_mask rg_hash_group_match(rg_hash_group g, uint8_t tag)
{
	uint64_t x = g ^ (UINT64_C(0x0101010101010101) * tag);
	return (x - UINT64_C(0x0101010101010101)) & ~x & UINT64_C(0x8080808080808080);
}
// 0x80 exactly: top bit set and bit six clear.
RGINLINE rg_hash_mask rg_hash_group_match_empty(rg_hash_group g)
{
	return g & (~g << 1) & UINT64_C(0x8080808080808080);
}
RGINLINE rg_hash_mask rg_hash_group_match_free(rg_hash_group g) { return g & UINT64_C(0x8080808080808080); }
RGINLINE unsigned rg_hash_mask_leading_zeros(rg_hash_mask m) { return rg_hash_clz64(m) >> 3; }
#endif

// Lowest matching lane, and the mask with that lane cleared.
RGINLINE unsigned rg_hash_mask_first(rg_hash_mask m) { return rg_hash_ctz64(m) >> RG_HASH_MASK_LANE_SHIFT; }
RGINLINE rg_hash_mask rg_hash_mask_clear_first(rg_hash_mask m) { return m & (m - 1u); }
// Bit of lane 0 (the home slot) in a mask.
#define RG_HASH_MASK_LANE0 ((rg_hash_mask)(RG_HASH_MASK_LANE_SHIFT ? 0x80u : 0x1u))

RGINLINE uint8_t rg_hash_tag(uint64_t hash) { return (uint8_t)((hash >> 25) & 0x7Fu); }

// Writes a control byte and its mirror past the end of the table.
RGINLINE void rg_hash_ctrl_set(uint8_t* ctrl, size_t cap, size_t index, uint8_t value)
{
	ctrl[index] = value;
	ctrl[((index - RG_HASH_GROUP_WIDTH) & (cap - 1)) + RG_HASH_GROUP_WIDTH] = value;
}

RGINLINE void rg_hash_ctrl_reset(uint8_t* ctrl, size_t cap)
{
	memset(ctrl, (int)RG_HASH_CTRL_EMPTY, cap + RG_HASH_GROUP_WIDTH);
}

// Entries the table admits before it must grow or rebuild.
RGINLINE size_t rg_hash_growth_for(size_t cap)
{
	return cap / (size_t)RG_HASH_MAX_LOAD_DEN * (size_t)RG_HASH_MAX_LOAD_NUM +
	       cap % (size_t)RG_HASH_MAX_LOAD_DEN * (size_t)RG_HASH_MAX_LOAD_NUM / (size_t)RG_HASH_MAX_LOAD_DEN;
}

// First empty or deleted slot on the key's probe sequence. The caller has
// established that the key is absent.
RGINLINE size_t rg_hash_find_free_slot(const uint8_t* ctrl, size_t cap, uint64_t hash)
{
	size_t mask = cap - 1;
	size_t pos = (size_t)hash & mask;
	size_t step = RG_HASH_GROUP_WIDTH;
	for (;;)
	{
		rg_hash_mask free_lanes = rg_hash_group_match_free(rg_hash_group_load(ctrl + pos));
		if (free_lanes)
		{
			return (pos + rg_hash_mask_first(free_lanes)) & mask;
		}
		pos = (pos + step) & mask;
		step += RG_HASH_GROUP_WIDTH;
	}
}

// Marks a slot unoccupied. It becomes EMPTY when no probe sequence can have
// skipped past it (some group window around it already holds an empty byte),
// otherwise DELETED so later lookups keep probing. Returns 1 for EMPTY.
RGINLINE int rg_hash_ctrl_remove(uint8_t* ctrl, size_t cap, size_t index)
{
	size_t mask = cap - 1;
	size_t before = (index - RG_HASH_GROUP_WIDTH) & mask;
	rg_hash_mask empty_after = rg_hash_group_match_empty(rg_hash_group_load(ctrl + index));
	rg_hash_mask empty_before = rg_hash_group_match_empty(rg_hash_group_load(ctrl + before));
	int never_full = empty_before != 0 && empty_after != 0 &&
	                 rg_hash_mask_first(empty_after) + rg_hash_mask_leading_zeros(empty_before) < RG_HASH_GROUP_WIDTH;
	rg_hash_ctrl_set(ctrl, cap, index, (uint8_t)(never_full ? RG_HASH_CTRL_EMPTY : RG_HASH_CTRL_DELETED));
	return never_full;
}

// =============================================================================
// TABLE DEFINITIONS
// =============================================================================

// Shared by the map and set macros: lookup by prepared hash. Expands to a loop
// that returns the entry index or RG_HASH_INVALID. The home lane is tested
// before the generic match loop: its entry address does not depend on the
// control bytes, so a predicted hit issues the entry load while the control
// group is still arriving, and the two cache misses overlap. A miss still ends
// on the control group alone. (Prefetching the home entry instead measured no
// better on hits and worse on misses.)
#define RG_HASH_FIND_LOOP(table, entries_field, key, hash, eq_call) \
	do \
	{ \
		if ((table)->cap == 0) \
		{ \
			return RG_HASH_INVALID; \
		} \
		size_t _rg_mask = (table)->cap - 1; \
		size_t _rg_pos = (size_t)(hash) & _rg_mask; \
		size_t _rg_step = RG_HASH_GROUP_WIDTH; \
		uint8_t _rg_tag = rg_hash_tag(hash); \
		rg_hash_group _rg_group = rg_hash_group_load((table)->ctrl + _rg_pos); \
		rg_hash_mask _rg_matches = rg_hash_group_match(_rg_group, _rg_tag); \
		if (_rg_matches & RG_HASH_MASK_LANE0) \
		{ \
			if ((table)->entries_field[_rg_pos].hash == (hash) && eq_call((table)->entries_field[_rg_pos].key, (key))) \
			{ \
				return _rg_pos; \
			} \
			_rg_matches &= ~(rg_hash_mask)RG_HASH_MASK_LANE0; \
		} \
		for (;;) \
		{ \
			while (_rg_matches) \
			{ \
				size_t _rg_idx = (_rg_pos + rg_hash_mask_first(_rg_matches)) & _rg_mask; \
				if ((table)->entries_field[_rg_idx].hash == (hash) && eq_call((table)->entries_field[_rg_idx].key, (key))) \
				{ \
					return _rg_idx; \
				} \
				_rg_matches = rg_hash_mask_clear_first(_rg_matches); \
			} \
			if (rg_hash_group_match_empty(_rg_group)) \
			{ \
				return RG_HASH_INVALID; \
			} \
			_rg_pos = (_rg_pos + _rg_step) & _rg_mask; \
			_rg_step += RG_HASH_GROUP_WIDTH; \
			_rg_group = rg_hash_group_load((table)->ctrl + _rg_pos); \
			_rg_matches = rg_hash_group_match(_rg_group, _rg_tag); \
		} \
	} while (0)

// Allocates control bytes and entries for new_cap slots and reinserts the
// occupied entries of the old arrays from their stored hashes. Leaves the table
// unchanged when the arena cannot satisfy the allocation.
#define RG_HASH_REBUILD_BODY(table, EntryType, new_cap) \
	EntryType* _rg_old_entries = (table)->entries; \
	size_t _rg_old_cap = (table)->cap; \
	size_t _rg_arena_used = (table)->arena->used; \
	uint8_t* _rg_ctrl = (uint8_t*)rg_arena_alloc_aligned((table)->arena, (new_cap) + RG_HASH_GROUP_WIDTH, 16); \
	EntryType* _rg_entries = _rg_ctrl == NULL ? NULL : (EntryType*)rg_arena_alloc_array((table)->arena, sizeof(EntryType), (new_cap), RG_ALIGNOF(EntryType)); \
	if (_rg_entries == NULL) \
	{ \
		/* Give back the control bytes when the entries did not fit. */ \
		(table)->arena->used = _rg_arena_used; \
		return 0; \
	} \
	rg_hash_ctrl_reset(_rg_ctrl, (new_cap)); \
	memset(_rg_entries, 0, (new_cap) * sizeof(EntryType)); \
	(table)->ctrl = _rg_ctrl; \
	(table)->entries = _rg_entries; \
	(table)->cap = (new_cap); \
	(table)->growth_left = rg_hash_growth_for(new_cap); \
	if (_rg_old_entries != NULL) \
	{ \
		for (size_t _rg_i = 0; _rg_i < _rg_old_cap; _rg_i++) \
		{ \
			if (_rg_old_entries[_rg_i].hash != 0) \
			{ \
				size_t _rg_slot = rg_hash_find_free_slot(_rg_ctrl, (new_cap), _rg_old_entries[_rg_i].hash); \
				rg_hash_ctrl_set(_rg_ctrl, (new_cap), _rg_slot, rg_hash_tag(_rg_old_entries[_rg_i].hash)); \
				_rg_entries[_rg_slot] = _rg_old_entries[_rg_i]; \
			} \
		} \
	} \
	(table)->growth_left -= (table)->count; \
	return 1;

// Makes room for one more new key. A table whose live entries fit within half
// the load limit of its size is only clogged with tombstones and is rebuilt at
// the same size; otherwise the table grows to the smallest capacity whose load
// limit admits count + 1 entries, at least doubling. Returns 0 when no such
// capacity exists or the arena cannot provide it.
#define RG_HASH_MAKE_ROOM_BODY(table, rebuild_fn) \
	size_t _rg_needed = rg_hash_capacity_for((table)->count + 1); \
	if (_rg_needed == 0) \
	{ \
		return 0; \
	} \
	if ((table)->cap != 0 && (table)->count + 1 <= rg_hash_growth_for((table)->cap) / 2) \
	{ \
		return rebuild_fn((table), (table)->cap); \
	} \
	if ((table)->cap != 0 && _rg_needed <= (table)->cap) \
	{ \
		if ((table)->cap > SIZE_MAX / 2) \
		{ \
			return 0; \
		} \
		_rg_needed = (table)->cap * 2; \
	} \
	return rebuild_fn((table), _rg_needed);

/**
 * @brief Define a typed hash map
 * @param key_type Key type
 * @param value_type Value type
 * @param name Map type name
 * @param hash_fn Hash function (uint64_t fn(key_type))
 * @param eq_fn Equality function (int fn(key_type a, key_type b))
 */
#define RG_HASH_MAP_DEFINE(key_type, value_type, name, hash_fn, eq_fn) \
	typedef key_type name##_Key; \
	typedef value_type name##_Value; \
	typedef struct name##_Entry \
	{ \
		uint64_t hash; \
		key_type key; \
		value_type value; \
	} name##_Entry; \
	typedef struct name \
	{ \
		name##_Entry* entries; \
		uint8_t* ctrl; \
		size_t cap; \
		size_t count; \
		size_t growth_left; \
		RgArena* arena; \
	} name; \
	RGINLINE uint64_t rg_hash_map_hash_##name(name##_Key key) \
	{ \
		return (hash_fn)(key); \
	} \
	RGINLINE int rg_hash_map_eq_##name(name##_Key a, name##_Key b) \
	{ \
		return (eq_fn)(a, b); \
	} \
	RGINLINE void rg_hash_map_init_##name(name* map, RgArena* arena) \
	{ \
		map->entries = NULL; \
		map->ctrl = NULL; \
		map->cap = 0; \
		map->count = 0; \
		map->growth_left = 0; \
		map->arena = arena; \
	} \
	RGINLINE void rg_hash_map_clear_##name(name* map) \
	{ \
		if (map->entries != NULL) \
		{ \
			memset(map->entries, 0, map->cap * sizeof(name##_Entry)); \
			rg_hash_ctrl_reset(map->ctrl, map->cap); \
			map->growth_left = rg_hash_growth_for(map->cap); \
		} \
		map->count = 0; \
	} \
	RGINLINE void rg_hash_map_free_##name(name* map) \
	{ \
		map->entries = NULL; \
		map->ctrl = NULL; \
		map->cap = 0; \
		map->count = 0; \
		map->growth_left = 0; \
		map->arena = NULL; \
	} \
	RGINLINE size_t rg_hash_map_find_index_hashed_##name(const name* map, name##_Key key, uint64_t hash) \
	{ \
		RG_HASH_FIND_LOOP(map, entries, key, hash, rg_hash_map_eq_##name); \
	} \
	RGINLINE size_t rg_hash_map_find_index_##name(const name* map, name##_Key key) \
	{ \
		if (map->cap == 0) \
		{ \
			return RG_HASH_INVALID; \
		} \
		return rg_hash_map_find_index_hashed_##name( \
		    map, key, rg_hash_prepare(rg_hash_map_hash_##name(key))); \
	} \
	RGINLINE name##_Value* rg_hash_map_get_ptr_##name(name* map, name##_Key key) \
	{ \
		size_t idx = rg_hash_map_find_index_##name(map, key); \
		if (idx == RG_HASH_INVALID) \
		{ \
			return NULL; \
		} \
		return &map->entries[idx].value; \
	} \
	RGINLINE const name##_Value* rg_hash_map_get_ptr_const_##name(const name* map, name##_Key key) \
	{ \
		size_t idx = rg_hash_map_find_index_##name(map, key); \
		if (idx == RG_HASH_INVALID) \
		{ \
			return NULL; \
		} \
		return &map->entries[idx].value; \
	} \
	RG_HASH_COLD int rg_hash_map_rehash_##name(name* map, size_t new_cap) \
	{ \
		RG_HASH_REBUILD_BODY(map, name##_Entry, new_cap) \
	} \
	RG_HASH_COLD int rg_hash_map_make_room_##name(name* map) \
	{ \
		RG_HASH_MAKE_ROOM_BODY(map, rg_hash_map_rehash_##name) \
	} \
	RGINLINE int rg_hash_map_reserve_##name(name* map, size_t min_count) \
	{ \
		RG_HASH_ASSERT(map->arena != NULL); \
		if (map->arena == NULL) \
		{ \
			return 0; \
		} \
		size_t new_cap = rg_hash_capacity_for(min_count); \
		if (new_cap == 0) \
		{ \
			return min_count == 0; \
		} \
		if (new_cap <= map->cap) \
		{ \
			return 1; \
		} \
		return rg_hash_map_rehash_##name(map, new_cap); \
	} \
	RGINLINE int rg_hash_map_put_##name(name* map, name##_Key key, name##_Value value) \
	{ \
		RG_HASH_ASSERT(map->arena != NULL); \
		uint64_t hash = rg_hash_prepare(rg_hash_map_hash_##name(key)); \
		size_t idx = rg_hash_map_find_index_hashed_##name(map, key, hash); \
		if (idx != RG_HASH_INVALID) \
		{ \
			map->entries[idx].value = value; \
			return 0; \
		} \
		if (map->growth_left == 0 && !rg_hash_map_make_room_##name(map)) \
		{ \
			return -1; \
		} \
		RG_HASH_ASSERT(map->growth_left > 0); \
		idx = rg_hash_find_free_slot(map->ctrl, map->cap, hash); \
		map->growth_left -= map->ctrl[idx] == RG_HASH_CTRL_EMPTY; \
		rg_hash_ctrl_set(map->ctrl, map->cap, idx, rg_hash_tag(hash)); \
		map->entries[idx].hash = hash; \
		map->entries[idx].key = key; \
		map->entries[idx].value = value; \
		map->count++; \
		return 1; \
	} \
	RGINLINE int rg_hash_map_remove_##name(name* map, name##_Key key) \
	{ \
		size_t idx = rg_hash_map_find_index_##name(map, key); \
		if (idx == RG_HASH_INVALID) \
		{ \
			return 0; \
		} \
		map->growth_left += (size_t)rg_hash_ctrl_remove(map->ctrl, map->cap, idx); \
		map->entries[idx].hash = 0; \
		map->count--; \
		return 1; \
	}

/**
 * @brief Define a typed hash set
 * @param key_type Key type
 * @param name Set type name
 * @param hash_fn Hash function (uint64_t fn(key_type))
 * @param eq_fn Equality function (int fn(key_type a, key_type b))
 */
#define RG_HASH_SET_DEFINE(key_type, name, hash_fn, eq_fn) \
	typedef key_type name##_Key; \
	typedef struct name##_Entry \
	{ \
		uint64_t hash; \
		key_type key; \
	} name##_Entry; \
	typedef struct name \
	{ \
		name##_Entry* entries; \
		uint8_t* ctrl; \
		size_t cap; \
		size_t count; \
		size_t growth_left; \
		RgArena* arena; \
	} name; \
	RGINLINE uint64_t rg_hash_set_hash_##name(name##_Key key) \
	{ \
		return (hash_fn)(key); \
	} \
	RGINLINE int rg_hash_set_eq_##name(name##_Key a, name##_Key b) \
	{ \
		return (eq_fn)(a, b); \
	} \
	RGINLINE void rg_hash_set_init_##name(name* set, RgArena* arena) \
	{ \
		set->entries = NULL; \
		set->ctrl = NULL; \
		set->cap = 0; \
		set->count = 0; \
		set->growth_left = 0; \
		set->arena = arena; \
	} \
	RGINLINE void rg_hash_set_clear_##name(name* set) \
	{ \
		if (set->entries != NULL) \
		{ \
			memset(set->entries, 0, set->cap * sizeof(name##_Entry)); \
			rg_hash_ctrl_reset(set->ctrl, set->cap); \
			set->growth_left = rg_hash_growth_for(set->cap); \
		} \
		set->count = 0; \
	} \
	RGINLINE void rg_hash_set_free_##name(name* set) \
	{ \
		set->entries = NULL; \
		set->ctrl = NULL; \
		set->cap = 0; \
		set->count = 0; \
		set->growth_left = 0; \
		set->arena = NULL; \
	} \
	RGINLINE size_t rg_hash_set_find_index_hashed_##name(const name* set, name##_Key key, uint64_t hash) \
	{ \
		RG_HASH_FIND_LOOP(set, entries, key, hash, rg_hash_set_eq_##name); \
	} \
	RGINLINE size_t rg_hash_set_find_index_##name(const name* set, name##_Key key) \
	{ \
		if (set->cap == 0) \
		{ \
			return RG_HASH_INVALID; \
		} \
		return rg_hash_set_find_index_hashed_##name( \
		    set, key, rg_hash_prepare(rg_hash_set_hash_##name(key))); \
	} \
	RGINLINE int rg_hash_set_contains_##name(const name* set, name##_Key key) \
	{ \
		return rg_hash_set_find_index_##name(set, key) != RG_HASH_INVALID; \
	} \
	RG_HASH_COLD int rg_hash_set_rehash_##name(name* set, size_t new_cap) \
	{ \
		RG_HASH_REBUILD_BODY(set, name##_Entry, new_cap) \
	} \
	RG_HASH_COLD int rg_hash_set_make_room_##name(name* set) \
	{ \
		RG_HASH_MAKE_ROOM_BODY(set, rg_hash_set_rehash_##name) \
	} \
	RGINLINE int rg_hash_set_reserve_##name(name* set, size_t min_count) \
	{ \
		RG_HASH_ASSERT(set->arena != NULL); \
		if (set->arena == NULL) \
		{ \
			return 0; \
		} \
		size_t new_cap = rg_hash_capacity_for(min_count); \
		if (new_cap == 0) \
		{ \
			return min_count == 0; \
		} \
		if (new_cap <= set->cap) \
		{ \
			return 1; \
		} \
		return rg_hash_set_rehash_##name(set, new_cap); \
	} \
	RGINLINE int rg_hash_set_insert_##name(name* set, name##_Key key) \
	{ \
		RG_HASH_ASSERT(set->arena != NULL); \
		uint64_t hash = rg_hash_prepare(rg_hash_set_hash_##name(key)); \
		size_t idx = rg_hash_set_find_index_hashed_##name(set, key, hash); \
		if (idx != RG_HASH_INVALID) \
		{ \
			return 0; \
		} \
		if (set->growth_left == 0 && !rg_hash_set_make_room_##name(set)) \
		{ \
			return -1; \
		} \
		RG_HASH_ASSERT(set->growth_left > 0); \
		idx = rg_hash_find_free_slot(set->ctrl, set->cap, hash); \
		set->growth_left -= set->ctrl[idx] == RG_HASH_CTRL_EMPTY; \
		rg_hash_ctrl_set(set->ctrl, set->cap, idx, rg_hash_tag(hash)); \
		set->entries[idx].hash = hash; \
		set->entries[idx].key = key; \
		set->count++; \
		return 1; \
	} \
	RGINLINE int rg_hash_set_remove_##name(name* set, name##_Key key) \
	{ \
		size_t idx = rg_hash_set_find_index_##name(set, key); \
		if (idx == RG_HASH_INVALID) \
		{ \
			return 0; \
		} \
		set->growth_left += (size_t)rg_hash_ctrl_remove(set->ctrl, set->cap, idx); \
		set->entries[idx].hash = 0; \
		set->count--; \
		return 1; \
	}

#define rg_hash_map_init(name, map, arena) rg_hash_map_init_##name((map), (arena))
#define rg_hash_map_reserve(name, map, min_count) rg_hash_map_reserve_##name((map), (min_count))
#define rg_hash_map_clear(name, map) rg_hash_map_clear_##name((map))
#define rg_hash_map_free(name, map) rg_hash_map_free_##name((map))
#define rg_hash_map_try_put(name, map, key, value) rg_hash_map_put_##name((map), (key), (value))
#define rg_hash_map_put(name, map, key, value, out_is_new) \
	do \
	{ \
		int _rg_put_result = rg_hash_map_try_put(name, (map), (key), (value)); \
		int* _rg_out = (int*)(out_is_new); \
		if (_rg_out != NULL) \
		{ \
			*_rg_out = _rg_put_result > 0; \
		} \
	} while (0)
#define rg_hash_map_remove(name, map, key, out_removed) \
	do \
	{ \
		int _rg_removed = rg_hash_map_remove_##name((map), (key)); \
		int* _rg_out = (int*)(out_removed); \
		if (_rg_out != NULL) \
		{ \
			*_rg_out = _rg_removed; \
		} \
	} while (0)
#define rg_hash_map_get_ptr(name, map, key) rg_hash_map_get_ptr_##name((map), (key))
#define rg_hash_map_get_ptr_const(name, map, key) rg_hash_map_get_ptr_const_##name((map), (key))
#define rg_hash_map_contains(name, map, key) (rg_hash_map_get_ptr_##name((map), (key)) != NULL)
#define rg_hash_map_count(map) ((map)->count)
#define rg_hash_map_capacity(map) ((map)->cap)
#define rg_hash_map_is_empty(map) ((map)->count == 0)

#define rg_hash_set_init(name, set, arena) rg_hash_set_init_##name((set), (arena))
#define rg_hash_set_reserve(name, set, min_count) rg_hash_set_reserve_##name((set), (min_count))
#define rg_hash_set_clear(name, set) rg_hash_set_clear_##name((set))
#define rg_hash_set_free(name, set) rg_hash_set_free_##name((set))
#define rg_hash_set_try_insert(name, set, key) rg_hash_set_insert_##name((set), (key))
#define rg_hash_set_insert(name, set, key, out_is_new) \
	do \
	{ \
		int _rg_insert_result = rg_hash_set_try_insert(name, (set), (key)); \
		int* _rg_out = (int*)(out_is_new); \
		if (_rg_out != NULL) \
		{ \
			*_rg_out = _rg_insert_result > 0; \
		} \
	} while (0)
#define rg_hash_set_remove(name, set, key, out_removed) \
	do \
	{ \
		int _rg_removed = rg_hash_set_remove_##name((set), (key)); \
		int* _rg_out = (int*)(out_removed); \
		if (_rg_out != NULL) \
		{ \
			*_rg_out = _rg_removed; \
		} \
	} while (0)
#define rg_hash_set_contains(name, set, key) rg_hash_set_contains_##name((set), (key))
#define rg_hash_set_count(set) ((set)->count)
#define rg_hash_set_capacity(set) ((set)->cap)
#define rg_hash_set_is_empty(set) ((set)->count == 0)

// =============================================================================
// IMPLEMENTATION
// =============================================================================

#define RG_HASH_PRIME1 11400714785074694791ULL
#define RG_HASH_PRIME2 14029467366897019727ULL
#define RG_HASH_PRIME3  1609587929392839161ULL
#define RG_HASH_PRIME4  9650029242287828579ULL
#define RG_HASH_PRIME5  2870177450012600261ULL

RGINLINE uint64_t rg_hash_rotl64(uint64_t value, int r)
{
	return (value << r) | (value >> (64 - r));
}

RGINLINE int rg_hash_is_big_endian(void)
{
#if defined(_WIN32)
	return 0;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    defined(__ORDER_LITTLE_ENDIAN__)
	return __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__;
#else
	const uint16_t value = 1;
	return ((const uint8_t*)&value)[0] == 0;
#endif
}

RGINLINE uint64_t rg_hash_bswap64(uint64_t value)
{
	return ((value & UINT64_C(0x00000000000000ff)) << 56) |
	       ((value & UINT64_C(0x000000000000ff00)) << 40) |
	       ((value & UINT64_C(0x0000000000ff0000)) << 24) |
	       ((value & UINT64_C(0x00000000ff000000)) << 8) |
	       ((value & UINT64_C(0x000000ff00000000)) >> 8) |
	       ((value & UINT64_C(0x0000ff0000000000)) >> 24) |
	       ((value & UINT64_C(0x00ff000000000000)) >> 40) |
	       ((value & UINT64_C(0xff00000000000000)) >> 56);
}

RGINLINE uint32_t rg_hash_bswap32(uint32_t value)
{
	return ((value & UINT32_C(0x000000ff)) << 24) |
	       ((value & UINT32_C(0x0000ff00)) << 8) |
	       ((value & UINT32_C(0x00ff0000)) >> 8) |
	       ((value & UINT32_C(0xff000000)) >> 24);
}

RGINLINE uint64_t rg_hash_read64(const void* ptr)
{
	uint64_t value;
	memcpy(&value, ptr, sizeof(value));
	return rg_hash_is_big_endian() ? rg_hash_bswap64(value) : value;
}

RGINLINE uint32_t rg_hash_read32(const void* ptr)
{
	uint32_t value;
	memcpy(&value, ptr, sizeof(value));
	return rg_hash_is_big_endian() ? rg_hash_bswap32(value) : value;
}

RGINLINE uint64_t rg_hash_round(uint64_t acc, uint64_t input)
{
	acc += input * RG_HASH_PRIME2;
	acc = rg_hash_rotl64(acc, 31);
	acc *= RG_HASH_PRIME1;
	return acc;
}

RGINLINE uint64_t rg_hash_merge_round(uint64_t acc, uint64_t val)
{
	val = rg_hash_round(0, val);
	acc ^= val;
	acc = acc * RG_HASH_PRIME1 + RG_HASH_PRIME4;
	return acc;
}

RGINLINE uint64_t rg_hash_avalanche(uint64_t hash)
{
	hash ^= hash >> 33;
	hash *= RG_HASH_PRIME2;
	hash ^= hash >> 29;
	hash *= RG_HASH_PRIME3;
	hash ^= hash >> 32;
	return hash;
}

RGINLINE uint64_t rg_hash_bytes(const void* data, size_t len, uint64_t seed)
{
	if (len == 0)
	{
		return rg_hash_avalanche(seed + RG_HASH_PRIME5);
	}
	RG_HASH_ASSERT(data != NULL);
	if (data == NULL)
	{
		return 0;
	}

	const uint8_t* ptr = (const uint8_t*)data;
	const uint8_t* end = ptr + len;
	uint64_t hash;

	if (len >= 32)
	{
		const uint8_t* limit = end - 32;
		uint64_t v1 = seed + RG_HASH_PRIME1 + RG_HASH_PRIME2;
		uint64_t v2 = seed + RG_HASH_PRIME2;
		uint64_t v3 = seed + 0;
		uint64_t v4 = seed - RG_HASH_PRIME1;

		do
		{
			v1 = rg_hash_round(v1, rg_hash_read64(ptr));
			ptr += 8;
			v2 = rg_hash_round(v2, rg_hash_read64(ptr));
			ptr += 8;
			v3 = rg_hash_round(v3, rg_hash_read64(ptr));
			ptr += 8;
			v4 = rg_hash_round(v4, rg_hash_read64(ptr));
			ptr += 8;
		} while (ptr <= limit);

		hash = rg_hash_rotl64(v1, 1) +
		       rg_hash_rotl64(v2, 7) +
		       rg_hash_rotl64(v3, 12) +
		       rg_hash_rotl64(v4, 18);

		hash = rg_hash_merge_round(hash, v1);
		hash = rg_hash_merge_round(hash, v2);
		hash = rg_hash_merge_round(hash, v3);
		hash = rg_hash_merge_round(hash, v4);
	}
	else
	{
		hash = seed + RG_HASH_PRIME5;
	}

	hash += len;

	while ((size_t)(end - ptr) >= 8)
	{
		uint64_t k1 = rg_hash_round(0, rg_hash_read64(ptr));
		hash ^= k1;
		hash = rg_hash_rotl64(hash, 27) * RG_HASH_PRIME1 + RG_HASH_PRIME4;
		ptr += 8;
	}

	if ((size_t)(end - ptr) >= 4)
	{
		hash ^= (uint64_t)rg_hash_read32(ptr) * RG_HASH_PRIME1;
		hash = rg_hash_rotl64(hash, 23) * RG_HASH_PRIME2 + RG_HASH_PRIME3;
		ptr += 4;
	}

	while (ptr < end)
	{
		hash ^= (*ptr) * RG_HASH_PRIME5;
		hash = rg_hash_rotl64(hash, 11) * RG_HASH_PRIME1;
		ptr++;
	}

	return rg_hash_avalanche(hash);
}

RGINLINE uint64_t rg_hash_str(const char* str)
{
	return rg_hash_bytes(str, strlen(str), RG_HASH_SEED);
}

RGINLINE uint64_t rg_hash_u32(uint32_t value)
{
	return rg_hash_avalanche(((uint64_t)value) + RG_HASH_SEED);
}

RGINLINE uint64_t rg_hash_u64(uint64_t value)
{
	return rg_hash_avalanche(value + RG_HASH_SEED);
}

RGINLINE uint64_t rg_hash_ptr(const void* ptr)
{
	return rg_hash_u64((uint64_t)(uintptr_t)ptr);
}

RGINLINE uint64_t rg_hash_u32_fast(uint32_t value)
{
	/* The high half of a 32x64 Fibonacci product is well mixed; return it as
	 * the low bits so power-of-two masking sees it. */
	return ((uint64_t)value * UINT64_C(0x9E3779B97F4A7C15)) >> 32;
}

/* Full 128-bit product of two 64-bit values; returns the high half. */
RGINLINE uint64_t rg_hash_mul_u64_wide(uint64_t a, uint64_t b, uint64_t* low)
{
#if defined(RG_HASH_USE_UMUL128)
	uint64_t high;
	*low = _umul128(a, b, &high);
	return high;
#elif defined(RG_HASH_USE_INT128)
	unsigned __int128 product = (unsigned __int128)a * (unsigned __int128)b;
	*low = (uint64_t)product;
	return (uint64_t)(product >> 64);
#else
	uint64_t a0 = (uint32_t)a;
	uint64_t a1 = a >> 32;
	uint64_t b0 = (uint32_t)b;
	uint64_t b1 = b >> 32;

	uint64_t product = a0 * b0;
	uint64_t word0 = (uint32_t)product;
	uint64_t carry = product >> 32;

	product = a1 * b0 + carry;
	uint64_t word1 = (uint32_t)product;
	uint64_t word2 = product >> 32;

	product = a0 * b1 + word1;
	*low = (product << 32) + word0;
	return a1 * b1 + word2 + (product >> 32);
#endif
}

RGINLINE uint64_t rg_hash_u64_fast(uint64_t value)
{
	/* A 64-bit multiply alone carries information upward only, so any single
	 * 64-bit product leaves keys that differ in their upper bits sharing low
	 * bits. The 128-bit product's high half depends on every input bit; XOR
	 * with the low half keeps small sequential keys well spread too. */
	uint64_t low;
	uint64_t high = rg_hash_mul_u64_wide(value, UINT64_C(0x9E3779B97F4A7C15), &low);
	return high ^ low;
}

RGINLINE uint64_t rg_hash_ptr_fast(const void* ptr)
{
	return rg_hash_u64_fast((uint64_t)(uintptr_t)ptr);
}

RGINLINE int rg_hash_eq_u32(uint32_t a, uint32_t b)
{
	return a == b;
}

RGINLINE int rg_hash_eq_u64(uint64_t a, uint64_t b)
{
	return a == b;
}

RGINLINE int rg_hash_eq_ptr(const void* a, const void* b)
{
	return a == b;
}

RGINLINE int rg_hash_eq_str(const char* a, const char* b)
{
	return strcmp(a, b) == 0;
}

RGINLINE uint64_t rg_hash_prepare(uint64_t hash)
{
	return hash == 0 ? 1 : hash;
}


RGINLINE size_t rg_hash_pow2_ceil(size_t value)
{
	if (value <= 1)
	{
		return 1;
	}

	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
#if SIZE_MAX > UINT32_MAX
	value |= value >> 32;
#endif
	return value + 1;
}

RGINLINE size_t rg_hash_capacity_for(size_t count)
{
	if (count == 0)
	{
		return 0;
	}

	const size_t numerator = (size_t)RG_HASH_MAX_LOAD_NUM;
	const size_t denominator = (size_t)RG_HASH_MAX_LOAD_DEN;
	if (count > (SIZE_MAX - (numerator - 1)) / denominator)
	{
		return 0;
	}

	size_t needed = (count * denominator + numerator - 1) / numerator;
	if (needed < (size_t)RG_HASH_MIN_CAP)
	{
		needed = (size_t)RG_HASH_MIN_CAP;
	}
	return rg_hash_pow2_ceil(needed);
}

RGINLINE int rg_hash_should_grow(size_t count, size_t cap)
{
	if (cap == 0 || count == SIZE_MAX)
	{
		return 1;
	}

	const size_t next_count = count + 1;
	const size_t numerator = (size_t)RG_HASH_MAX_LOAD_NUM;
	const size_t denominator = (size_t)RG_HASH_MAX_LOAD_DEN;
	if (next_count > SIZE_MAX / denominator || cap > SIZE_MAX / numerator)
	{
		size_t needed = rg_hash_capacity_for(next_count);
		return needed == 0 || needed > cap;
	}
	return next_count * denominator > cap * numerator;
}

#endif // RG_HASH_H
