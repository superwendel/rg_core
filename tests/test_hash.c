// rg_hash public API correctness tests

#define RG_HASH_ASSERT(condition) ((void)sizeof(condition))

#include "../src/rg_hash.h"
#include "../src/rg_hash.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int tests_run;
static int tests_failed;

#define CHECK(condition)                                                \
	do                                                                  \
	{                                                                   \
		tests_run++;                                                    \
		if (!(condition))                                               \
		{                                                               \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			tests_failed++;                                             \
		}                                                               \
	} while (0)

RGINLINE uint64_t test_zero_hash(uint32_t key)
{
	RG_UNUSED(key);
	return 0;
}

RGINLINE uint64_t test_identity_hash(uint32_t key)
{
	return key;
}

static int test_hash_calls;

RGINLINE uint64_t test_counting_hash(uint32_t key)
{
	test_hash_calls++;
	return rg_hash_u32(key);
}

RG_HASH_MAP_DEFINE(uint32_t, uint32_t, U32Map, rg_hash_u32, rg_hash_eq_u32);
RG_HASH_MAP_DEFINE(const char*, uint32_t, StrMap, rg_hash_str, rg_hash_eq_str);
RG_HASH_MAP_DEFINE(uint32_t, uint32_t, CollisionMap, test_zero_hash, rg_hash_eq_u32);
RG_HASH_SET_DEFINE(uint32_t, U32Set, rg_hash_u32, rg_hash_eq_u32);
RG_HASH_SET_DEFINE(uint32_t, CollisionSet, test_zero_hash, rg_hash_eq_u32);
RG_HASH_MAP_DEFINE(uint32_t, uint32_t, ProbeMap, test_identity_hash, rg_hash_eq_u32);
RG_HASH_SET_DEFINE(uint32_t, ProbeSet, test_identity_hash, rg_hash_eq_u32);
RG_HASH_MAP_DEFINE(uint32_t, uint32_t, FastMap, rg_hash_u32_fast, rg_hash_eq_u32);
RG_HASH_SET_DEFINE(uint64_t, FastSet, rg_hash_u64_fast, rg_hash_eq_u64);
RG_HASH_MAP_DEFINE(uint32_t, uint32_t, CountMap, test_counting_hash, rg_hash_eq_u32);
RG_HASH_SET_DEFINE(uint32_t, CountSet, test_counting_hash, rg_hash_eq_u32);

static RgArena arena;

static uint32_t rng_next(uint32_t* state)
{
	uint32_t value = *state;
	value ^= value << 13;
	value ^= value >> 17;
	value ^= value << 5;
	*state = value;
	return value;
}

static void test_hash_functions(void)
{
	static const char unaligned[] = "xabc";
	const char* text = "rg_hash";
	const char* fox = "The quick brown fox jumps over the lazy dog";

	CHECK(rg_hash_bytes(NULL, 0, 0) == UINT64_C(0xef46db3751d8e999));
	CHECK(rg_hash_str("") == UINT64_C(0xef46db3751d8e999));
	CHECK(rg_hash_str("a") == UINT64_C(0xd24ec4f1a98c6e5b));
	CHECK(rg_hash_str("abc") == UINT64_C(0x44bc2cf5ad770999));
	CHECK(rg_hash_str(fox) == UINT64_C(0x0b242d361fda71bc));
	CHECK(rg_hash_bytes(text, strlen(text), RG_HASH_SEED) == rg_hash_str(text));
	CHECK(rg_hash_bytes(unaligned + 1, 3, 0) == rg_hash_str("abc"));
	CHECK(rg_hash_u32(42) == rg_hash_u64(42));
	CHECK(rg_hash_bswap32(UINT32_C(0x12345678)) == UINT32_C(0x78563412));
	CHECK(rg_hash_bswap64(UINT64_C(0x0123456789abcdef)) ==
	      UINT64_C(0xefcdab8967452301));

	int value = 42;
	CHECK(rg_hash_ptr(&value) == rg_hash_u64((uint64_t)(uintptr_t)&value));
	CHECK(rg_hash_ptr_fast(&value) == rg_hash_u64_fast((uint64_t)(uintptr_t)&value));
	CHECK(rg_hash_u32_fast(0) == 0 && rg_hash_u64_fast(0) == 0);
	CHECK(rg_hash_u32_fast(1) == UINT64_C(0x9E3779B9));
	CHECK(rg_hash_u32_fast(1) != rg_hash_u32_fast(2));
	CHECK(rg_hash_u64_fast(1) == UINT64_C(0x9E3779B97F4A7C15));
	{
		/* The wide multiply must agree with its portable form. */
		uint64_t low = 0;
		uint64_t high = rg_hash_mul_u64_wide(UINT64_C(0xFFFFFFFFFFFFFFFF), UINT64_C(0xFFFFFFFFFFFFFFFF), &low);
		CHECK(high == UINT64_C(0xFFFFFFFFFFFFFFFE) && low == 1);
		high = rg_hash_mul_u64_wide(UINT64_C(0x123456789ABCDEF0), UINT64_C(0x9E3779B97F4A7C15), &low);
		CHECK(high == UINT64_C(0x0B403F44F1289150) && low == UINT64_C(0xC93A7B79AEDA89B0));
	}
	CHECK(rg_hash_eq_u32(7, 7));
	CHECK(!rg_hash_eq_u64(7, 8));
	CHECK(rg_hash_eq_ptr(&value, &value));
	CHECK(rg_hash_eq_str("same", "same"));
}

static void test_capacity_helpers(void)
{
	CHECK(rg_hash_pow2_ceil(0) == 1);
	CHECK(rg_hash_pow2_ceil(1) == 1);
	CHECK(rg_hash_pow2_ceil(9) == 16);
	CHECK(rg_hash_capacity_for(0) == 0);
	CHECK(rg_hash_capacity_for(1) >= RG_HASH_MIN_CAP);
	CHECK((rg_hash_capacity_for(100) & (rg_hash_capacity_for(100) - 1)) == 0);
	CHECK(rg_hash_capacity_for(SIZE_MAX) == 0);
	CHECK(rg_hash_should_grow(0, 0));
	CHECK(!rg_hash_should_grow(0, RG_HASH_MIN_CAP));
	CHECK(rg_hash_should_grow(SIZE_MAX, SIZE_MAX));
}

static void test_map_basic(void)
{
	rg_arena_reset(&arena);

	U32Map map;
	rg_hash_map_init(U32Map, &map, &arena);
	CHECK(rg_hash_map_is_empty(&map));
	CHECK(rg_hash_map_reserve(U32Map, &map, 100));
	CHECK(rg_hash_map_capacity(&map) >= 100);

	int is_new = 0;
	rg_hash_map_put(U32Map, &map, 10u, 100u, &is_new);
	CHECK(is_new == 1);
	rg_hash_map_put(U32Map, &map, 20u, 200u, &is_new);
	CHECK(is_new == 1);
	CHECK(rg_hash_map_count(&map) == 2);
	CHECK(rg_hash_map_contains(U32Map, &map, 10u));

	uint32_t* value = rg_hash_map_get_ptr(U32Map, &map, 10u);
	CHECK(value != NULL && *value == 100u);
	rg_hash_map_put(U32Map, &map, 10u, 111u, &is_new);
	CHECK(is_new == 0);
	value = rg_hash_map_get_ptr(U32Map, &map, 10u);
	CHECK(value != NULL && *value == 111u);

	const U32Map* const_map = &map;
	const uint32_t* const_value = rg_hash_map_get_ptr_const(U32Map, const_map, 10u);
	CHECK(const_value != NULL && *const_value == 111u);

	int removed = 0;
	rg_hash_map_remove(U32Map, &map, 20u, &removed);
	CHECK(removed == 1);
	rg_hash_map_remove(U32Map, &map, 20u, &removed);
	CHECK(removed == 0);
	CHECK(rg_hash_map_count(&map) == 1);

	size_t capacity = rg_hash_map_capacity(&map);
	rg_hash_map_clear(U32Map, &map);
	CHECK(rg_hash_map_is_empty(&map));
	CHECK(rg_hash_map_capacity(&map) == capacity);
	rg_hash_map_free(U32Map, &map);
	CHECK(map.entries == NULL && map.arena == NULL);
}

static void test_map_random(void)
{
	enum
	{
		KEY_COUNT = 1024,
		OP_COUNT = 30000
	};
	uint8_t present[KEY_COUNT] = {0};
	uint32_t expected_values[KEY_COUNT] = {0};
	uint32_t state = UINT32_C(0x12345678);

	rg_arena_reset(&arena);
	U32Map map;
	rg_hash_map_init(U32Map, &map, &arena);

	for (size_t i = 0; i < OP_COUNT; i++)
	{
		uint32_t key = rng_next(&state) & (KEY_COUNT - 1);
		uint32_t value = rng_next(&state);
		uint32_t action = rng_next(&state) % 3u;
		if (action == 0)
		{
			int is_new = 0;
			rg_hash_map_put(U32Map, &map, key, value, &is_new);
			CHECK(is_new == (present[key] == 0));
			present[key] = 1;
			expected_values[key] = value;
		}
		else if (action == 1)
		{
			int removed = 0;
			rg_hash_map_remove(U32Map, &map, key, &removed);
			CHECK(removed == (present[key] != 0));
			present[key] = 0;
		}
		else
		{
			uint32_t* found = rg_hash_map_get_ptr(U32Map, &map, key);
			CHECK((found != NULL) == (present[key] != 0));
			if (found != NULL)
				CHECK(*found == expected_values[key]);
		}
	}

	size_t expected_count = 0;
	for (size_t key = 0; key < KEY_COUNT; key++)
	{
		expected_count += present[key] != 0;
		uint32_t* found = rg_hash_map_get_ptr(U32Map, &map, (uint32_t)key);
		CHECK((found != NULL) == (present[key] != 0));
		if (found != NULL)
			CHECK(*found == expected_values[key]);
	}
	CHECK(rg_hash_map_count(&map) == expected_count);
}

static void test_map_collisions(void)
{
	rg_arena_reset(&arena);
	CollisionMap map;
	rg_hash_map_init(CollisionMap, &map, &arena);

	for (uint32_t key = 0; key < 64; key++)
		rg_hash_map_put(CollisionMap, &map, key, key * 3, NULL);
	CHECK(rg_hash_map_count(&map) == 64);

	for (uint32_t key = 0; key < 64; key++)
	{
		uint32_t* value = rg_hash_map_get_ptr(CollisionMap, &map, key);
		CHECK(value != NULL && *value == key * 3);
	}
	for (uint32_t key = 0; key < 64; key += 2)
		rg_hash_map_remove(CollisionMap, &map, key, NULL);
	for (uint32_t key = 0; key < 64; key++)
		CHECK(rg_hash_map_contains(CollisionMap, &map, key) == ((key & 1u) != 0));
}

static void test_string_map(void)
{
	static const char* keys[] = {"apple", "banana", "cherry", "date", "elderberry"};
	rg_arena_reset(&arena);
	StrMap map;
	rg_hash_map_init(StrMap, &map, &arena);

	for (size_t i = 0; i < RG_ARRAY_COUNT(keys); i++)
		rg_hash_map_put(StrMap, &map, keys[i], (uint32_t)i, NULL);
	for (size_t i = 0; i < RG_ARRAY_COUNT(keys); i++)
	{
		uint32_t* value = rg_hash_map_get_ptr(StrMap, &map, keys[i]);
		CHECK(value != NULL && *value == (uint32_t)i);
	}
	CHECK(rg_hash_map_get_ptr(StrMap, &map, "missing") == NULL);
}

static void test_set_random(void)
{
	enum
	{
		KEY_COUNT = 4096,
		OP_COUNT = 30000
	};
	uint8_t present[KEY_COUNT] = {0};
	uint32_t state = UINT32_C(0xdeadbeef);

	rg_arena_reset(&arena);
	U32Set set;
	rg_hash_set_init(U32Set, &set, &arena);
	CHECK(rg_hash_set_reserve(U32Set, &set, 256));

	for (size_t i = 0; i < OP_COUNT; i++)
	{
		uint32_t key = rng_next(&state) & (KEY_COUNT - 1);
		uint32_t action = rng_next(&state) % 3u;
		if (action == 0)
		{
			int is_new = 0;
			rg_hash_set_insert(U32Set, &set, key, &is_new);
			CHECK(is_new == (present[key] == 0));
			present[key] = 1;
		}
		else if (action == 1)
		{
			int removed = 0;
			rg_hash_set_remove(U32Set, &set, key, &removed);
			CHECK(removed == (present[key] != 0));
			present[key] = 0;
		}
		else
		{
			CHECK(rg_hash_set_contains(U32Set, &set, key) == (present[key] != 0));
		}
	}

	size_t expected_count = 0;
	for (size_t key = 0; key < KEY_COUNT; key++)
		expected_count += present[key] != 0;
	CHECK(rg_hash_set_count(&set) == expected_count);

	rg_hash_set_clear(U32Set, &set);
	CHECK(rg_hash_set_is_empty(&set));
	rg_hash_set_free(U32Set, &set);
	CHECK(set.entries == NULL && set.arena == NULL);
}

static void test_set_collisions(void)
{
	rg_arena_reset(&arena);
	CollisionSet set;
	rg_hash_set_init(CollisionSet, &set, &arena);

	for (uint32_t key = 0; key < 64; key++)
		rg_hash_set_insert(CollisionSet, &set, key, NULL);
	for (uint32_t key = 0; key < 64; key += 2)
		rg_hash_set_remove(CollisionSet, &set, key, NULL);
	for (uint32_t key = 0; key < 64; key++)
		CHECK(rg_hash_set_contains(CollisionSet, &set, key) == ((key & 1u) != 0));
}

static void test_collision_probe_and_exhausted_update(void)
{
	rg_arena_reset(&arena);
	ProbeMap map;
	ProbeSet set;
	rg_hash_map_init(ProbeMap, &map, &arena);
	rg_hash_set_init(ProbeSet, &set, &arena);
	CHECK(rg_hash_map_reserve(ProbeMap, &map, 1));
	CHECK(rg_hash_set_reserve(ProbeSet, &set, 1));
	CHECK(map.cap == RG_HASH_MIN_CAP && set.cap == RG_HASH_MIN_CAP);

	// Identity-hashed keys that all start in the same slot, so every one after
	// the first probes past occupied slots; fill the table to its load limit.
	const size_t limit = rg_hash_growth_for(map.cap);
	uint32_t keys[64];
	CHECK(limit < RG_ARRAY_COUNT(keys));
	for (size_t i = 0; i <= limit; ++i) keys[i] = 6u + (uint32_t)(i * map.cap);
	for (size_t i = 0; i < limit; ++i)
	{
		CHECK(rg_hash_map_try_put(ProbeMap, &map, keys[i], keys[i] * 3) == 1);
		CHECK(rg_hash_set_try_insert(ProbeSet, &set, keys[i]) == 1);
	}
	CHECK(map.cap == RG_HASH_MIN_CAP && map.count == limit && map.growth_left == 0);
	CHECK(set.cap == RG_HASH_MIN_CAP && set.count == limit && set.growth_left == 0);

	// Updating an existing key at the limit allocates nothing even when the arena
	// is exhausted; inserting a new key fails cleanly and changes nothing.
	size_t saved_capacity = arena.capacity;
	size_t saved_used = arena.used;
	arena.capacity = arena.used;
	ProbeMap_Entry* saved_map_entries = map.entries;
	ProbeSet_Entry* saved_set_entries = set.entries;
	CHECK(rg_hash_map_try_put(ProbeMap, &map, keys[3], 999) == 0);
	CHECK(rg_hash_set_try_insert(ProbeSet, &set, keys[3]) == 0);
	CHECK(arena.used == saved_used && map.cap == RG_HASH_MIN_CAP && set.cap == RG_HASH_MIN_CAP);

	CHECK(rg_hash_map_try_put(ProbeMap, &map, keys[limit], 114) == -1);
	CHECK(rg_hash_set_try_insert(ProbeSet, &set, keys[limit]) == -1);
	CHECK(map.entries == saved_map_entries && map.count == limit && map.cap == RG_HASH_MIN_CAP);
	CHECK(set.entries == saved_set_entries && set.count == limit && set.cap == RG_HASH_MIN_CAP);
	CHECK(arena.used == saved_used);
	for (size_t i = 0; i < limit; ++i)
	{
		uint32_t* value = rg_hash_map_get_ptr(ProbeMap, &map, keys[i]);
		CHECK(value != NULL && *value == (i == 3 ? 999u : keys[i] * 3));
		CHECK(rg_hash_set_contains(ProbeSet, &set, keys[i]));
	}
	CHECK(!rg_hash_map_contains(ProbeMap, &map, keys[limit]));
	CHECK(!rg_hash_set_contains(ProbeSet, &set, keys[limit]));

	// Growth must preserve every collided key and the value updated while full.
	arena.capacity = saved_capacity;
	CHECK(rg_hash_map_try_put(ProbeMap, &map, keys[limit], 114) == 1);
	CHECK(map.cap == 2 * RG_HASH_MIN_CAP && map.count == limit + 1);
	CHECK(rg_hash_set_try_insert(ProbeSet, &set, keys[limit]) == 1);
	CHECK(set.cap == 2 * RG_HASH_MIN_CAP && set.count == limit + 1);
	for (size_t i = 0; i <= limit; ++i)
	{
		uint32_t* value = rg_hash_map_get_ptr(ProbeMap, &map, keys[i]);
		CHECK(value != NULL && *value == (i == 3 ? 999u : (i == limit ? 114u : keys[i] * 3)));
		CHECK(rg_hash_set_contains(ProbeSet, &set, keys[i]));
	}

	// Occupancy is visible through entries[i].hash for callers that iterate.
	size_t live = 0;
	for (size_t i = 0; i < map.cap; ++i) live += map.entries[i].hash != 0;
	CHECK(live == map.count);
}

static void test_tombstone_churn(void)
{
	// Removing and reinserting at a fixed size must not degrade lookups or leak
	// capacity: tombstones are reclaimed by a same-size rebuild when needed.
	rg_arena_reset(&arena);
	ProbeMap map;
	rg_hash_map_init(ProbeMap, &map, &arena);
	CHECK(rg_hash_map_reserve(ProbeMap, &map, 40));
	const size_t cap = map.cap;
	for (uint32_t round = 0; round < 200; ++round)
	{
		for (uint32_t i = 0; i < 40; ++i)
		{
			uint32_t key = round * 1000u + i;
			CHECK(rg_hash_map_try_put(ProbeMap, &map, key, key + 1) == 1);
		}
		CHECK(map.count == 40 && map.cap == cap);
		for (uint32_t i = 0; i < 40; ++i)
		{
			uint32_t key = round * 1000u + i;
			const uint32_t* value = rg_hash_map_get_ptr(ProbeMap, &map, key);
			CHECK(value != NULL && *value == key + 1);
			CHECK(!rg_hash_map_contains(ProbeMap, &map, key + 500u));
		}
		for (uint32_t i = 0; i < 40; i += 2)
		{
			int removed = 0;
			rg_hash_map_remove(ProbeMap, &map, round * 1000u + i, &removed);
			CHECK(removed == 1);
		}
		CHECK(map.count == 20);
		size_t live = 0;
		for (size_t i = 0; i < map.cap; ++i) live += map.entries[i].hash != 0;
		CHECK(live == 20);
		for (uint32_t i = 1; i < 40; i += 2)
		{
			int removed = 0;
			rg_hash_map_remove(ProbeMap, &map, round * 1000u + i, &removed);
			CHECK(removed == 1);
			CHECK(!rg_hash_map_contains(ProbeMap, &map, round * 1000u + i));
		}
		CHECK(map.count == 0);
	}
	CHECK(map.cap == cap);
}

/* Count the slots a key pattern occupies under an 8,192-slot mask. Random keys
 * would fill about 3,220 of them; structured keys must not collapse. */
static uint32_t test_distinct_slots_u64(const uint64_t* keys, size_t count)
{
	static uint8_t seen[8192];
	uint32_t distinct = 0;
	memset(seen, 0, sizeof(seen));
	for (size_t i = 0; i < count; i++)
	{
		uint32_t slot = (uint32_t)(rg_hash_u64_fast(keys[i]) & 8191u);
		distinct += seen[slot] == 0;
		seen[slot] = 1;
	}
	return distinct;
}

static uint32_t test_distinct_slots_u32(const uint32_t* keys, size_t count)
{
	static uint8_t seen[8192];
	uint32_t distinct = 0;
	memset(seen, 0, sizeof(seen));
	for (size_t i = 0; i < count; i++)
	{
		uint32_t slot = (uint32_t)(rg_hash_u32_fast(keys[i]) & 8191u);
		distinct += seen[slot] == 0;
		seen[slot] = 1;
	}
	return distinct;
}

static void test_fast_hash_distribution(void)
{
	enum
	{
		COUNT = 4096,
		MIN_DISTINCT = 2800
	};
	static uint64_t keys64[COUNT];
	static uint32_t keys32[COUNT];

	/* Keys differing only above bit 32: a bare multiply leaves the low bits constant. */
	for (uint64_t i = 0; i < COUNT; i++) keys64[i] = (i << 48) | 7u;
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);
	for (uint64_t i = 0; i < COUNT; i++) keys64[i] = i << 32;
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);
	/* Aligned addresses: constant low bits, varying middle bits. */
	for (uint64_t i = 0; i < COUNT; i++) keys64[i] = UINT64_C(0x7f0000000000) + i * 16u;
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);
	for (uint64_t i = 0; i < COUNT; i++) keys64[i] = UINT64_C(0x5555f000) + i * 64u;
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);
	for (uint64_t i = 0; i < COUNT; i++) keys64[i] = i;
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);
	for (uint64_t i = 0; i < COUNT; i++) keys64[i] = i * 4096u;
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);
	/* Upper and middle bits moving together cancel under a fold-then-multiply hash. */
	for (uint64_t i = 0; i < COUNT; i++) keys64[i] = (i << 48) | (i << 16) | 7u;
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);
	/* Packed float pairs and generation/index identifiers. */
	for (uint32_t i = 0; i < COUNT; i++)
	{
		float x = (float)i;
		float y = (float)i * 0.5f;
		uint32_t xb, yb;
		memcpy(&xb, &x, sizeof(xb));
		memcpy(&yb, &y, sizeof(yb));
		keys64[i] = ((uint64_t)xb << 32) | yb;
	}
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);
	for (uint32_t i = 0; i < COUNT; i++)
	{
		float x = (float)i;
		uint32_t xb;
		memcpy(&xb, &x, sizeof(xb));
		keys64[i] = ((uint64_t)xb << 32) | xb;
	}
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);
	for (uint64_t i = 0; i < COUNT; i++) keys64[i] = ((i % 7u) << 32) | (i / 7u);
	CHECK(test_distinct_slots_u64(keys64, COUNT) >= MIN_DISTINCT);

	for (uint32_t i = 0; i < COUNT; i++) keys32[i] = (i << 16) | 7u;
	CHECK(test_distinct_slots_u32(keys32, COUNT) >= MIN_DISTINCT);
	for (uint32_t i = 0; i < COUNT; i++) keys32[i] = i;
	CHECK(test_distinct_slots_u32(keys32, COUNT) >= MIN_DISTINCT);
	for (uint32_t i = 0; i < COUNT; i++) keys32[i] = i * 64u;
	CHECK(test_distinct_slots_u32(keys32, COUNT) >= MIN_DISTINCT);
	for (uint32_t i = 0; i < COUNT; i++) keys32[i] = i * 2654435761u;
	CHECK(test_distinct_slots_u32(keys32, COUNT) >= MIN_DISTINCT);

	/* The table itself must stay near one probe per lookup on structured keys. */
	rg_arena_reset(&arena);
	FastSet set;
	rg_hash_set_init(FastSet, &set, &arena);
	CHECK(rg_hash_set_reserve(FastSet, &set, COUNT));
	for (uint64_t i = 0; i < COUNT; i++) keys64[i] = (i << 48) | 7u;
	for (size_t i = 0; i < COUNT; i++) CHECK(rg_hash_set_try_insert(FastSet, &set, keys64[i]) == 1);
	size_t probes = 0;
	for (size_t i = 0; i < COUNT; i++)
	{
		uint64_t hash = rg_hash_prepare(rg_hash_u64_fast(keys64[i]));
		size_t mask = set.cap - 1;
		size_t idx = (size_t)(hash & mask);
		probes++;
		while (!(set.entries[idx].hash == hash && set.entries[idx].key == keys64[i]))
		{
			idx = (idx + 1) & mask;
			probes++;
		}
	}
	CHECK(probes < (size_t)COUNT * 3u);
}

static void test_fast_hash_tables(void)
{
	enum
	{
		KEY_COUNT = 2048,
		OP_COUNT = 40000
	};
	uint8_t present[KEY_COUNT] = {0};
	uint32_t expected_values[KEY_COUNT] = {0};
	uint32_t state = UINT32_C(0x0badf00d);

	rg_arena_reset(&arena);
	FastMap map;
	FastSet set;
	rg_hash_map_init(FastMap, &map, &arena);
	rg_hash_set_init(FastSet, &set, &arena);

	for (size_t i = 0; i < OP_COUNT; i++)
	{
		uint32_t key = rng_next(&state) & (KEY_COUNT - 1);
		uint32_t value = rng_next(&state);
		uint32_t action = rng_next(&state) % 3u;
		/* Zero keys exercise the zero-hash remapping of both fast functions. */
		uint64_t set_key = (uint64_t)key << 20;
		if (action == 0)
		{
			int is_new = 0;
			rg_hash_map_put(FastMap, &map, key, value, &is_new);
			CHECK(is_new == (present[key] == 0));
			CHECK(rg_hash_set_try_insert(FastSet, &set, set_key) == (present[key] == 0));
			present[key] = 1;
			expected_values[key] = value;
		}
		else if (action == 1)
		{
			int removed = 0;
			rg_hash_map_remove(FastMap, &map, key, &removed);
			CHECK(removed == (present[key] != 0));
			rg_hash_set_remove(FastSet, &set, set_key, &removed);
			CHECK(removed == (present[key] != 0));
			present[key] = 0;
		}
		else
		{
			uint32_t* found = rg_hash_map_get_ptr(FastMap, &map, key);
			CHECK((found != NULL) == (present[key] != 0));
			if (found != NULL)
				CHECK(*found == expected_values[key]);
			CHECK(rg_hash_set_contains(FastSet, &set, set_key) == (present[key] != 0));
		}
	}
	size_t expected_count = 0;
	for (size_t key = 0; key < KEY_COUNT; key++)
		expected_count += present[key] != 0;
	CHECK(rg_hash_map_count(&map) == expected_count);
	CHECK(rg_hash_set_count(&set) == expected_count);
}

// Every table operation hashes its key exactly once, including an insertion
// that grows the table: rehashing reuses the stored hashes.
static void test_single_hash_per_operation(void)
{
	rg_arena_reset(&arena);
	CountMap map;
	rg_hash_map_init(CountMap, &map, &arena);
	CountSet set;
	rg_hash_set_init(CountSet, &set, &arena);

	test_hash_calls = 0;
	CHECK(rg_hash_map_try_put(CountMap, &map, 1u, 10u) == 1);
	CHECK(test_hash_calls == 1);
	CHECK(rg_hash_map_try_put(CountMap, &map, 1u, 11u) == 0);
	CHECK(test_hash_calls == 2);
	CHECK(rg_hash_map_get_ptr(CountMap, &map, 1u) != NULL);
	CHECK(test_hash_calls == 3);
	CHECK(rg_hash_map_get_ptr(CountMap, &map, 2u) == NULL);
	CHECK(test_hash_calls == 4);
	CHECK(rg_hash_map_remove_CountMap(&map, 1u) == 1);
	CHECK(test_hash_calls == 5);
	CHECK(rg_hash_map_remove_CountMap(&map, 1u) == 0);
	CHECK(test_hash_calls == 6);

	test_hash_calls = 0;
	size_t cap_before = rg_hash_map_capacity(&map);
	uint32_t inserted = 0;
	while (rg_hash_map_capacity(&map) == cap_before)
	{
		CHECK(rg_hash_map_try_put(CountMap, &map, 100u + inserted, inserted) == 1);
		inserted++;
	}
	CHECK(inserted > 0);
	CHECK(test_hash_calls == (int)inserted);
	for (uint32_t i = 0; i < inserted; i++)
	{
		uint32_t* value = rg_hash_map_get_ptr(CountMap, &map, 100u + i);
		CHECK(value != NULL && *value == i);
	}

	test_hash_calls = 0;
	CHECK(rg_hash_set_try_insert(CountSet, &set, 1u) == 1);
	CHECK(test_hash_calls == 1);
	CHECK(rg_hash_set_try_insert(CountSet, &set, 1u) == 0);
	CHECK(test_hash_calls == 2);
	CHECK(rg_hash_set_contains(CountSet, &set, 1u));
	CHECK(test_hash_calls == 3);
	CHECK(rg_hash_set_remove_CountSet(&set, 1u) == 1);
	CHECK(test_hash_calls == 4);

	test_hash_calls = 0;
	cap_before = rg_hash_set_capacity(&set);
	inserted = 0;
	while (rg_hash_set_capacity(&set) == cap_before)
	{
		CHECK(rg_hash_set_try_insert(CountSet, &set, 100u + inserted) == 1);
		inserted++;
	}
	CHECK(inserted > 0);
	CHECK(test_hash_calls == (int)inserted);
	for (uint32_t i = 0; i < inserted; i++)
		CHECK(rg_hash_set_contains(CountSet, &set, 100u + i));
}

static void test_allocation_failure(void)
{
	RgArena tiny = rg_arena_create(KB(1));
	CHECK(tiny.memory != NULL);

	U32Map map;
	rg_hash_map_init(U32Map, &map, &tiny);
	CHECK(!rg_hash_map_reserve(U32Map, &map, 1000));
	CHECK(map.entries == NULL);
	CHECK(map.count == 0);
	CHECK(map.cap == 0);
	CHECK(rg_hash_map_try_put(U32Map, &map, 1, 2) == 1);

	CHECK(rg_hash_map_count(&map) == 1);
	rg_arena_free(&tiny);

	char byte = 0;
	RgArena full = {0};
	full.memory = &byte;
	U32Map failed_map;
	rg_hash_map_init(U32Map, &failed_map, &full);
	CHECK(rg_hash_map_try_put(U32Map, &failed_map, 1, 2) == -1);
	CHECK(failed_map.entries == NULL && failed_map.count == 0);
}

int main(void)
{
	if (rg_malloc(MB(32)) != 0)
	{
		printf("Failed to initialize the memory pool.\n");
		return 1;
	}
	CHECK(rg_total() >= MB(32));
	CHECK(rg_used() == 0);
	CHECK(rg_remaining() == rg_total());
	arena = rg_arena_create(MB(16));
	if (arena.memory == NULL)
	{
		printf("Failed to create the test arena.\n");
		rg_free();
		return 1;
	}

	test_hash_functions();
	test_capacity_helpers();
	test_map_basic();
	test_map_random();
	test_map_collisions();
	test_string_map();
	test_set_random();
	test_set_collisions();
	test_collision_probe_and_exhausted_update();
	test_tombstone_churn();
	test_fast_hash_distribution();
	test_fast_hash_tables();
	test_single_hash_per_operation();
	test_allocation_failure();

	rg_arena_free(&arena);
	rg_free();
	printf("rg_hash: %d checks, %d failures\n", tests_run, tests_failed);
	return tests_failed == 0 ? 0 : 1;
}
