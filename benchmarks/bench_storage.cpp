// Compile with the source tree's src directory on the include path.
// Link bench_sink.c compiled separately without /GL. Optional argv[1] filters cases.
#include "bench_common.h"
#include <rg_containers.h>
#include <rg_hash.h>
#include <rg_random.h>
#include <rg_string.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <vector>

static RgArena storage_arena;

static void require(bool condition)
{
	if (!condition)
	{
		fprintf(stderr, "Storage benchmark validation failed\n");
		exit(1);
	}
}

static uint32_t input_next(uint32_t& state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

// Runtime bounds cross an opaque function boundary so constant propagation
// cannot turn these cases into the separate constant-bound measurements.
#if defined(_MSC_VER)
#define STORAGE_NOINLINE __declspec(noinline)
#else
#define STORAGE_NOINLINE __attribute__((noinline))
#endif
STORAGE_NOINLINE static u32 runtime_u32(RgRng* rng, u32 bound)
{
	return rg_random_bounded_u32(rng, bound);
}

STORAGE_NOINLINE static u64 runtime_u64(RgRng* rng, u64 bound)
{
	return rg_random_bounded_u64(rng, bound);
}

enum class RngMode { Constant10, Constant1024, Runtime, Shrinking };
struct RngCase { u64 bound; RngMode mode; };

template<bool Wide, RngMode Mode>
static double rng_sample(void* context, uint64_t* checksum)
{
	const RngCase& config = *static_cast<RngCase*>(context);
	(void)rg_bench_consume(&config.bound, sizeof(config.bound));
	const size_t count = 2u * 1024u * 1024u;
	RgRng rng;
	rg_rng_seed(&rng, 1234);
	u64 sum = 0;
	double start = rg_bench_now_ns();
	for (size_t i = 0; i < count; ++i)
	{
		if constexpr (Wide)
		{
			if constexpr (Mode == RngMode::Constant10) sum += rg_random_bounded_u64(&rng, 10);
			else if constexpr (Mode == RngMode::Constant1024) sum += rg_random_bounded_u64(&rng, 1024);
			else if constexpr (Mode == RngMode::Shrinking) sum += runtime_u64(&rng, count - i);
			else sum += runtime_u64(&rng, config.bound);
		}
		else
		{
			if constexpr (Mode == RngMode::Constant10) sum += rg_random_bounded_u32(&rng, 10);
			else if constexpr (Mode == RngMode::Constant1024) sum += rg_random_bounded_u32(&rng, 1024);
			else if constexpr (Mode == RngMode::Shrinking) sum += runtime_u32(&rng, (u32)(count - i));
			else sum += runtime_u32(&rng, (u32)config.bound);
		}
	}
	double elapsed = rg_bench_now_ns() - start;
	*checksum = rg_bench_consume(&sum, sizeof(sum)) ^ rg_bench_consume(rng.state, sizeof(rng.state));
	return elapsed / (double)count;
}

static double shuffle_sample(void*, uint64_t* checksum)
{
	const size_t count = 1024u * 1024u;
	const size_t repeats = 2;
	std::vector<u32> data(count);
	RgRng rng;
	rg_rng_seed(&rng, 1234);
	double elapsed = 0;
	*checksum = 0;
	for (size_t r = 0; r < repeats; ++r)
	{
		for (size_t i = 0; i < count; ++i) data[i] = (u32)i;
		double start = rg_bench_now_ns();
		rg_random_shuffle(data.data(), count, sizeof(data[0]), &rng);
		elapsed += rg_bench_now_ns() - start;
		*checksum ^= rg_bench_consume(data.data(), data.size() * sizeof(data[0]));
		u64 sum = 0;
		for (u32 value : data) sum += value;
		require(sum == (u64)count * (count - 1) / 2);
	}
	return elapsed / (double)(count * repeats);
}

template<bool Wide>
static void run_rng_cases(const char* width)
{
	char name[128];
	RngCase config = {0, RngMode::Runtime};
	snprintf(name, sizeof(name), "storage.rng.%s.constant10", width);
	rg_bench_run(name, rng_sample<Wide, RngMode::Constant10>, &config);
	snprintf(name, sizeof(name), "storage.rng.%s.constant1024", width);
	rg_bench_run(name, rng_sample<Wide, RngMode::Constant1024>, &config);
	const u64 bounds[] = {10, 1000000, Wide ? UINT64_C(0x8000000000000001) : UINT64_C(0x80000001),
	                      Wide ? UINT64_MAX : UINT32_MAX};
	const char* labels[] = {"runtime10", "runtime1m", "rejection", "near_full"};
	for (size_t i = 0; i < 4; ++i)
	{
		config.bound = bounds[i];
		snprintf(name, sizeof(name), "storage.rng.%s.%s", width, labels[i]);
		rg_bench_run(name, rng_sample<Wide, RngMode::Runtime>, &config);
	}
	snprintf(name, sizeof(name), "storage.rng.%s.shrinking", width);
	rg_bench_run(name, rng_sample<Wide, RngMode::Shrinking>, &config);
}

struct Record32 { uint64_t words[4]; };
static_assert(sizeof(Record32) == 32, "Benchmark record size changed");
RG_ARRAY_DEFINE(int, BenchIntArray);
RG_ARRAY_DEFINE(Record32, BenchRecordArray);
RG_SMALLVEC_DEFINE(int, BenchSmallVec, 8);

template<class T> struct ArrayOps;
template<> struct ArrayOps<int>
{
	using Array = BenchIntArray;
	static void reserve(Array& a, size_t count) { rg_array_reserve(int, &a, count); }
	static void push(Array& a, int value) { rg_array_push(int, &a, value); }
	static int input(size_t i) { return (int)(i * 1664525u + 1013904223u); }
};
template<> struct ArrayOps<Record32>
{
	using Array = BenchRecordArray;
	static void reserve(Array& a, size_t count) { rg_array_reserve(Record32, &a, count); }
	static void push(Array& a, Record32 value) { rg_array_push(Record32, &a, value); }
	static Record32 input(size_t i) { return {{i, i * 31, i ^ 0x12345678u, i * 7919}}; }
};

struct ArrayCase { size_t count; bool reserve; };

template<class T>
static double array_sample(void* context, uint64_t* checksum)
{
	const ArrayCase& config = *static_cast<ArrayCase*>(context);
	std::vector<T> input(config.count);
	for (size_t i = 0; i < input.size(); ++i) input[i] = ArrayOps<T>::input(i);
	uint64_t expected = rg_bench_consume(input.data(), input.size() * sizeof(T));
	const size_t repeats = std::max<size_t>(1, MB(32) / (config.count * sizeof(T)));
	double elapsed = 0;
	*checksum = 0;
	for (size_t r = 0; r < repeats; ++r)
	{
		rg_arena_reset(&storage_arena);
		typename ArrayOps<T>::Array array;
		rg_array_init(&array, &storage_arena);
		if (config.reserve) ArrayOps<T>::reserve(array, config.count);
		double start = rg_bench_now_ns();
		for (size_t i = 0; i < config.count; ++i) ArrayOps<T>::push(array, input[i]);
		elapsed += rg_bench_now_ns() - start;
		require(array.len == config.count);
		uint64_t actual = rg_bench_consume(array.data, array.len * sizeof(T));
		require(actual == expected);
		*checksum += actual;
	}
	return elapsed / (double)(config.count * repeats);
}

static double smallvec_sample(void* context, uint64_t* checksum)
{
	const size_t count = *static_cast<size_t*>(context);
	const size_t instances = 32768;
	const size_t repeats = 16;
	// Initialize only after the array has its final address: small vectors contain
	// pointers into themselves while using inline storage.
	std::unique_ptr<BenchSmallVec[]> vectors(new BenchSmallVec[instances]);
	std::vector<int> output(instances * count);
	double elapsed = 0;
	*checksum = 0;
	for (size_t r = 0; r < repeats; ++r)
	{
		rg_arena_reset(&storage_arena);
		for (size_t i = 0; i < instances; ++i) rg_smallvec_init(&vectors[i], &storage_arena);
		double start = rg_bench_now_ns();
		for (size_t i = 0; i < instances; ++i)
			for (size_t j = 0; j < count; ++j)
				rg_smallvec_push(int, &vectors[i], (int)(i + j));
		elapsed += rg_bench_now_ns() - start;
		(void)rg_bench_consume(vectors.get(), instances * sizeof(vectors[0]));
		for (size_t i = 0; i < instances; ++i)
		{
			require(vectors[i].len == count);
			for (size_t j = 0; j < count; ++j)
			{
				int value = vectors[i].data[j];
				require(value == (int)(i + j));
				output[i * count + j] = value;
			}
		}
		*checksum += rg_bench_consume(output.data(), output.size() * sizeof(output[0]));
	}
	return elapsed / (double)(instances * count * repeats);
}

struct StringCopyCase { size_t old_len; size_t replacement_len; };

STORAGE_NOINLINE static void setup_string_copies(RgString* strings, size_t count,
                                                const char* initial, size_t old_len)
{
	rg_arena_reset(&storage_arena);
	for (size_t i = 0; i < count; ++i)
		rgs_init_with_n(&strings[i], &storage_arena, initial, old_len);
}

static double string_copy_sample(void* context, uint64_t* checksum)
{
	const StringCopyCase& config = *static_cast<StringCopyCase*>(context);
	(void)rg_bench_consume(&config, sizeof(config));
	const size_t instances = MB(8) / (config.old_len + config.replacement_len + 2);
	const size_t repeats = 8;
	std::vector<RgString> strings(instances);
	std::vector<char> initial(config.old_len);
	std::vector<char> replacement(config.replacement_len);
	for (size_t i = 0; i < initial.size(); ++i) initial[i] = (char)(i * 17u + 3u);
	for (size_t i = 0; i < replacement.size(); ++i) replacement[i] = (char)(i * 31u + 5u);
	uint64_t expected = rg_bench_consume(replacement.data(), replacement.size());

	// Commit and touch the complete allocation range before timing the copies.
	const size_t arena_bytes = instances * (config.old_len + config.replacement_len + 2);
	rg_arena_reset(&storage_arena);
	void* warmed = rg_arena_alloc_aligned(&storage_arena, arena_bytes, RG_ALIGNOF(char));
	require(warmed != NULL);
	memset(warmed, 0, arena_bytes);
	(void)rg_bench_consume(warmed, arena_bytes);

	double elapsed = 0;
	*checksum = 0;
	for (size_t r = 0; r < repeats; ++r)
	{
		setup_string_copies(strings.data(), instances, initial.data(), initial.size());
		for (const RgString& s : strings)
			require(s.len == config.old_len && s.cap == config.old_len);
		double start = rg_bench_now_ns();
		for (size_t i = 0; i < instances; ++i)
			rgs_copy_n(&strings[i], replacement.data(), replacement.size());
		elapsed += rg_bench_now_ns() - start;
		for (const RgString& s : strings)
		{
			require(s.len == config.replacement_len &&
			        s.cap == std::max(config.old_len, config.replacement_len));
			require(s.data[s.len] == '\0');
			uint64_t actual = rg_bench_consume(s.data, s.len);
			require(actual == expected);
			*checksum += actual;
		}
	}
	return elapsed / (double)(instances * repeats);
}

RG_HASH_MAP_DEFINE(uint32_t, uint32_t, BenchU32Map, rg_hash_u32, rg_hash_eq_u32);
RG_HASH_MAP_DEFINE(const char*, uint32_t, BenchStringMap, rg_hash_str, rg_hash_eq_str);
RG_HASH_SET_DEFINE(uint32_t, BenchU32Set, rg_hash_u32, rg_hash_eq_u32);
RG_HASH_SET_DEFINE(const char*, BenchStringSet, rg_hash_str, rg_hash_eq_str);

template<class Table> struct HashOps;
#define BENCH_MAP_OPS(Table) \
	template<> struct HashOps<Table> { \
		using Key = Table##_Key; \
		static constexpr bool is_map = true; \
		static void init(Table& t) { rg_hash_map_init(Table, &t, &storage_arena); } \
		static int reserve(Table& t, size_t n) { return rg_hash_map_reserve(Table, &t, n); } \
		static int put(Table& t, Key key, uint32_t value) { return rg_hash_map_try_put(Table, &t, key, value); } \
		static uint64_t get(Table& t, Key key) { uint32_t* p = rg_hash_map_get_ptr(Table, &t, key); return p ? (uint64_t)*p + 1 : 0; } \
		static void remove(Table& t, Key key) { rg_hash_map_remove(Table, &t, key, NULL); } \
		static uint64_t entry_value(const Table##_Entry& entry) { return entry.value; } \
	};
#define BENCH_SET_OPS(Table) \
	template<> struct HashOps<Table> { \
		using Key = Table##_Key; \
		static constexpr bool is_map = false; \
		static void init(Table& t) { rg_hash_set_init(Table, &t, &storage_arena); } \
		static int reserve(Table& t, size_t n) { return rg_hash_set_reserve(Table, &t, n); } \
		static int put(Table& t, Key key, uint32_t) { return rg_hash_set_try_insert(Table, &t, key); } \
		static uint64_t get(Table& t, Key key) { return (uint64_t)rg_hash_set_contains(Table, &t, key); } \
		static void remove(Table& t, Key key) { rg_hash_set_remove(Table, &t, key, NULL); } \
		static uint64_t entry_value(const Table##_Entry&) { return 0; } \
	};
BENCH_MAP_OPS(BenchU32Map)
BENCH_MAP_OPS(BenchStringMap)
BENCH_SET_OPS(BenchU32Set)
BENCH_SET_OPS(BenchStringSet)
#undef BENCH_MAP_OPS
#undef BENCH_SET_OPS

enum class HashMode { Reserved, Growing, Update, Hit, Miss, Remove };
template<class Table> struct HashCase
{
	size_t count;
	size_t capacity;
	std::vector<typename HashOps<Table>::Key> keys;
	std::vector<std::array<char, 33>> strings;
};

// Keep untimed insertion code out of the operation being measured. In
// particular, insertion changes must not affect lookup's register allocation.
template<class Table, HashMode Mode>
STORAGE_NOINLINE static void setup_hash_table(Table* table, const HashCase<Table>* config)
{
	using Ops = HashOps<Table>;
	rg_arena_reset(&storage_arena);
	Ops::init(*table);
	if constexpr (Mode != HashMode::Growing)
	{
		require(Ops::reserve(*table, config->capacity * RG_HASH_MAX_LOAD_NUM / RG_HASH_MAX_LOAD_DEN) != 0);
		require(table->cap == config->capacity);
	}
	if constexpr (Mode != HashMode::Growing && Mode != HashMode::Reserved)
		for (size_t i = 0; i < config->count; ++i) require(Ops::put(*table, config->keys[i], (uint32_t)i) == 1);
}

template<class Table, HashMode Mode>
static double hash_sample(void* context, uint64_t* checksum)
{
	const HashCase<Table>& config = *static_cast<HashCase<Table>*>(context);
	using Ops = HashOps<Table>;
	const size_t repeats = std::max<size_t>(1, 262144u / config.count);
	double elapsed = 0;
	*checksum = 0;
	for (size_t r = 0; r < repeats; ++r)
	{
		Table table;
		setup_hash_table<Table, Mode>(&table, &config);
		uint64_t found_sum = 0;
		double start = rg_bench_now_ns();
		if constexpr (Mode == HashMode::Reserved || Mode == HashMode::Growing)
			for (size_t i = 0; i < config.count; ++i) Ops::put(table, config.keys[i], (uint32_t)i);
		else if constexpr (Mode == HashMode::Update)
			for (size_t i = 0; i < config.count; ++i) Ops::put(table, config.keys[i], (uint32_t)i ^ 0xa5a5a5a5u);
		else if constexpr (Mode == HashMode::Hit)
			for (size_t i = 0; i < config.count; ++i) found_sum += Ops::get(table, config.keys[i]);
		else if constexpr (Mode == HashMode::Miss)
			for (size_t i = 0; i < config.count; ++i) found_sum += Ops::get(table, config.keys[config.count + i]);
		else if constexpr (Mode == HashMode::Remove)
			for (size_t i = 0; i < config.count; ++i) Ops::remove(table, config.keys[i]);
		elapsed += rg_bench_now_ns() - start;
		require(table.count == (Mode == HashMode::Remove ? 0 : config.count));
		if constexpr (Mode == HashMode::Miss) require(found_sum == 0);
		if constexpr (Mode == HashMode::Hit)
			require(found_sum == (Ops::is_map ? (uint64_t)config.count * (config.count + 1) / 2 : config.count));
		// Consume actual storage but exclude pointer addresses and struct padding
		// from the reported checksum, which must match between processes/builds.
		(void)rg_bench_consume(table.entries, table.cap * sizeof(table.entries[0]));
		uint64_t digest = found_sum;
		for (size_t i = 0; i < table.cap; ++i)
			if (table.entries[i].hash != 0)
				digest += table.entries[i].hash * (i + 1) ^ Ops::entry_value(table.entries[i]);
		*checksum += rg_bench_consume(&digest, sizeof(digest));
	}
	return elapsed / (double)(config.count * repeats);
}

template<class Table, HashMode Mode>
static void run_hash_case(const char* type, const char* operation, size_t capacity, size_t count)
{
	char name[160];
	snprintf(name, sizeof(name), "storage.hash.%s.%s.c%zu.n%zu", type, operation, capacity, count);
	if (!rg_bench_selected(name)) return;
	HashCase<Table> config;
	config.count = count;
	config.capacity = capacity;
	config.keys.resize(count * 2);
	std::vector<uint32_t> ids(count * 2);
	for (size_t i = 0; i < ids.size(); ++i) ids[i] = (uint32_t)i * 2654435761u;
	uint32_t state = 0x31415926u;
	for (size_t i = ids.size() - 1; i > 0; --i) std::swap(ids[i], ids[input_next(state) % (i + 1)]);
	if constexpr (std::is_same<typename HashOps<Table>::Key, const char*>::value)
	{
		config.strings.resize(ids.size());
		for (size_t i = 0; i < ids.size(); ++i)
		{
			uint32_t id = ids[i];
			snprintf(config.strings[i].data(), 33, "%08x%08x%08x%08x", id, id ^ 0x12345678u, id * 31, id * 7919);
			config.keys[i] = config.strings[i].data();
		}
	}
	else
		for (size_t i = 0; i < ids.size(); ++i) config.keys[i] = ids[i];
	rg_bench_run(name, hash_sample<Table, Mode>, &config);
}

template<class Table>
static void run_hash_cases(const char* type)
{
	for (size_t capacity : {size_t(8192), size_t(524288)})
	{
		run_hash_case<Table, HashMode::Reserved>(type, "insert_reserved", capacity, capacity / 2);
		run_hash_case<Table, HashMode::Growing>(type, "insert_growing", capacity, capacity / 2);
		run_hash_case<Table, HashMode::Update>(type, "update", capacity, capacity * 69 / 100);
		run_hash_case<Table, HashMode::Remove>(type, "remove", capacity, capacity / 2);
		for (size_t occupancy : {size_t(25), size_t(69)})
		{
			run_hash_case<Table, HashMode::Hit>(type, "hit", capacity, capacity * occupancy / 100);
			run_hash_case<Table, HashMode::Miss>(type, "miss", capacity, capacity * occupancy / 100);
		}
	}
}

int main(int argc, char** argv)
{
	rg_bench_init(argc, argv);
	require(rg_malloc(MB(256)) == 0);
	require(rg_total() >= MB(256) && rg_used() == 0 && rg_remaining() == rg_total());
	storage_arena = rg_arena_create(MB(128));
	require(storage_arena.memory != NULL);
	run_rng_cases<false>("u32");
	run_rng_cases<true>("u64");
	rg_bench_run("storage.rng.shuffle_u32", shuffle_sample, NULL);
	for (size_t count : {size_t(4096), size_t(262144)})
		for (bool reserve : {false, true})
		{
			ArrayCase config = {count, reserve};
			char name[128];
			snprintf(name, sizeof(name), "storage.array.int.%s.n%zu", reserve ? "reserved" : "growing", count);
			rg_bench_run(name, array_sample<int>, &config);
			snprintf(name, sizeof(name), "storage.array.record32.%s.n%zu", reserve ? "reserved" : "growing", count);
			rg_bench_run(name, array_sample<Record32>, &config);
		}
	size_t inline_count = 8, spill_count = 16;
	rg_bench_run("storage.smallvec.inline8", smallvec_sample, &inline_count);
	rg_bench_run("storage.smallvec.spill16", smallvec_sample, &spill_count);
	for (StringCopyCase config : {StringCopyCase{31, 32}, StringCopyCase{4096, 4097},
	                              StringCopyCase{4096, 8192}, StringCopyCase{65536, 65537}})
	{
		char name[128];
		snprintf(name, sizeof(name), "storage.string.copy_growth.old%zu.new%zu",
		         config.old_len, config.replacement_len);
		rg_bench_run(name, string_copy_sample, &config);
	}
	for (size_t len : {size_t(31), size_t(4096), size_t(65536)})
	{
		StringCopyCase config = {len, len};
		char name[128];
		snprintf(name, sizeof(name), "storage.string.copy_reuse.n%zu", len);
		rg_bench_run(name, string_copy_sample, &config);
	}
	run_hash_cases<BenchU32Map>("map_u32");
	run_hash_cases<BenchStringMap>("map_string32");
	run_hash_cases<BenchU32Set>("set_u32");
	run_hash_cases<BenchStringSet>("set_string32");
	rg_arena_free(&storage_arena);
	rg_free();
	return 0;
}
