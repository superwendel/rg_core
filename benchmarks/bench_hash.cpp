// rg_hash benchmark suite
//
// Usage:
//   build.bat bench_hash
//   ./build.sh bench            (Linux; pins to the lowest permitted CPU)
//
// Set RG_BENCH_DEPS to enable the optional stb_ds comparison.
//
// Three table sizes separate instruction cost (4,096 entries fit in L2) from
// memory cost (500,000 entries exceed L3 on most desktops). Lookups run in the
// insertion order, which favors node allocators whose nodes become a sequential
// stream, and in an independent shuffled order, which is how real workloads
// arrive. Misses use keys that are never inserted.
//
// Every timed pass returns a checksum that is compared with the expected value
// before any sample is accepted. Samples are calibrated to at least
// RG_BENCH_HASH_SAMPLE_MS of work, and the implementation order rotates every
// sample so no implementation always runs first.

#include "../src/rg_hash.h"

#ifdef RG_BENCH_STB_DS
#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>
#endif

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <unordered_map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static double get_time_ms(void)
{
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&freq);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
}
#else
#include <time.h>
#if defined(__linux__)
#include <sched.h>
#endif
static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}
#endif

static volatile uint64_t g_sink = 0;

enum
{
    SAMPLE_COUNT = 7
};

#ifndef RG_BENCH_HASH_SAMPLE_MS
#define RG_BENCH_HASH_SAMPLE_MS 20.0
#endif

// One timed measurement: elapsed milliseconds for `passes` repetitions of the
// operation over the workload, plus a checksum of every result produced.
struct Measurement
{
    double ms;
    uint64_t checksum;
};

static int pin_benchmark_thread(void)
{
#ifdef _WIN32
    DWORD processor_count = GetActiveProcessorCount(0);
    DWORD processor_index = processor_count > 2u ? 2u : 0u;
    DWORD_PTR mask = (DWORD_PTR)1u << processor_index;
    if (SetThreadAffinityMask(GetCurrentThread(), mask) == 0u)
    {
        return -1;
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    return (int)processor_index;
#elif defined(__linux__)
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0)
    {
        return -1;
    }
    for (int cpu = 0; cpu < CPU_SETSIZE; cpu++)
    {
        if (CPU_ISSET(cpu, &allowed))
        {
            cpu_set_t one;
            CPU_ZERO(&one);
            CPU_SET(cpu, &one);
            return sched_setaffinity(0, sizeof(one), &one) == 0 ? cpu : -1;
        }
    }
    return -1;
#else
    return -1;
#endif
}

// =============================================================================
// Workload
// =============================================================================

struct Workload
{
    size_t count;
    std::vector<uint32_t> keys;     // inserted keys, in insertion order
    std::vector<uint32_t> shuffled; // the same keys in an independent order
    std::vector<uint32_t> missing;  // distinct keys that are never inserted
    uint64_t value_sum;             // sum of every stored value, for checksums
};

typedef Measurement (*HashBenchFn)(const Workload& workload, size_t passes);

static uint32_t value_for(uint32_t key)
{
    return key ^ 0x9e3779b9u;
}

static uint32_t rng_next(uint32_t* state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void shuffle_keys(std::vector<uint32_t>& keys, uint32_t seed)
{
    for (size_t i = keys.size(); i > 1; i--)
    {
        seed = rng_next(&seed);
        std::swap(keys[i - 1], keys[seed % i]);
    }
}

static Workload make_workload(size_t count)
{
    Workload w;
    w.count = count;
    w.keys.resize(count);
    w.missing.resize(count);
    // Multiplication by an odd constant is a bijection on uint32_t, so these
    // two index ranges never produce the same key.
    for (size_t i = 0; i < count; i++)
    {
        w.keys[i] = static_cast<uint32_t>(i * 2654435761u);
        w.missing[i] = static_cast<uint32_t>((i + count) * 2654435761u);
    }
    shuffle_keys(w.keys, 0x31415926u);
    shuffle_keys(w.missing, 0x27182818u);
    w.shuffled = w.keys;
    shuffle_keys(w.shuffled, 0x9e3779b9u);
    w.value_sum = 0;
    for (size_t i = 0; i < count; i++)
    {
        w.value_sum += value_for(w.keys[i]);
    }
    return w;
}

// Expected checksums: hits sum their stored values once per pass; misses add
// nothing; insert counts each reported new key plus the final entry count
// (2 x count per pass); remove counts each reported removal plus the final
// entry count, which must be zero (count per pass).
enum CaseKind
{
    CASE_INSERT,
    CASE_FIND_HIT,
    CASE_FIND_MISS,
    CASE_REMOVE
};

static uint64_t expected_checksum(CaseKind kind, const Workload& w, size_t passes)
{
    switch (kind)
    {
        case CASE_INSERT: return 2u * static_cast<uint64_t>(w.count) * passes;
        case CASE_FIND_HIT: return w.value_sum * passes;
        case CASE_FIND_MISS: return 0;
        case CASE_REMOVE: return static_cast<uint64_t>(w.count) * passes;
    }
    return 0;
}

static int g_validation_failures = 0;

static void validate(const char* implementation, const char* op, size_t count,
                     const Measurement& m, uint64_t expected)
{
    if (m.checksum != expected)
    {
        printf("  VALIDATION FAILED: %s / %s / %zu entries: checksum %llu, expected %llu\n",
               implementation, op, count,
               static_cast<unsigned long long>(m.checksum),
               static_cast<unsigned long long>(expected));
        g_validation_failures++;
    }
}

// Passes per sample so one sample takes at least RG_BENCH_HASH_SAMPLE_MS.
static size_t calibrate(HashBenchFn fn, const Workload& w)
{
    size_t passes = 1;
    for (;;)
    {
        Measurement m = fn(w, passes);
        if (m.ms >= RG_BENCH_HASH_SAMPLE_MS || passes >= (size_t(1) << 20))
        {
            return passes;
        }
        passes *= 2;
    }
}

struct Implementation
{
    const char* name;
    HashBenchFn fn; // NULL when this implementation does not run the case
};

// Measure every implementation of one case with rotating order: sample r runs
// the implementations starting at index r, so each one leads equally often.
static void measure_case(const char* op, CaseKind kind, const Workload& w,
                         const Implementation* impls, size_t impl_count,
                         double* out_ns)
{
    std::vector<size_t> passes(impl_count, 0);
    std::vector<std::array<double, SAMPLE_COUNT> > samples(impl_count);
    for (size_t i = 0; i < impl_count; i++)
    {
        out_ns[i] = -1.0;
        if (impls[i].fn) passes[i] = calibrate(impls[i].fn, w);
    }
    for (size_t r = 0; r < SAMPLE_COUNT; r++)
    {
        for (size_t n = 0; n < impl_count; n++)
        {
            size_t i = (r + n) % impl_count;
            if (!impls[i].fn) continue;
            Measurement m = impls[i].fn(w, passes[i]);
            validate(impls[i].name, op, w.count, m, expected_checksum(kind, w, passes[i]));
            samples[i][r] = m.ms * 1000000.0 / (static_cast<double>(w.count) * passes[i]);
        }
    }
    for (size_t i = 0; i < impl_count; i++)
    {
        if (!impls[i].fn) continue;
        std::sort(samples[i].begin(), samples[i].end());
        out_ns[i] = samples[i][SAMPLE_COUNT / 2];
    }
}

// =============================================================================
// rg_hash map benchmarks
// =============================================================================

RG_HASH_MAP_DEFINE(uint32_t, uint32_t, U32Map, rg_hash_u32_fast, rg_hash_eq_u32);
// The avalanche hash costs more per key; compare it on lookups only.
RG_HASH_MAP_DEFINE(uint32_t, uint32_t, U32MixMap, rg_hash_u32, rg_hash_eq_u32);

static RgArena g_arena;

#define RG_BENCH_MAP(name, Type)                                                     \
    static Type name##_build(const Workload& w)                                      \
    {                                                                                \
        rg_arena_reset(&g_arena);                                                    \
        Type map;                                                                    \
        rg_hash_map_init(Type, &map, &g_arena);                                      \
        rg_hash_map_reserve(Type, &map, w.count);                                    \
        for (size_t i = 0; i < w.count; i++)                                         \
            rg_hash_map_put(Type, &map, w.keys[i], value_for(w.keys[i]), NULL);      \
        return map;                                                                  \
    }                                                                                \
    static Measurement name##_insert(const Workload& w, size_t passes)               \
    {                                                                                \
        Measurement m = {0.0, 0};                                                    \
        for (size_t p = 0; p < passes; p++)                                          \
        {                                                                            \
            rg_arena_reset(&g_arena);                                                \
            Type map;                                                                \
            rg_hash_map_init(Type, &map, &g_arena);                                  \
            rg_hash_map_reserve(Type, &map, w.count);                                \
            uint64_t inserted = 0;                                                  \
            double start = get_time_ms();                                           \
            for (size_t i = 0; i < w.count; i++)                                    \
                inserted += rg_hash_map_try_put(Type, &map, w.keys[i], value_for(w.keys[i])) == 1;\
            m.ms += get_time_ms() - start;                                          \
            m.checksum += inserted + rg_hash_map_count(&map);                       \
        }                                                                            \
        return m;                                                                    \
    }                                                                                \
    static Measurement name##_find(const Workload& w, size_t passes,                 \
                                   const std::vector<uint32_t>& q)                   \
    {                                                                                \
        Type map = name##_build(w);                                                  \
        Measurement m = {0.0, 0};                                                    \
        double start = get_time_ms();                                               \
        for (size_t p = 0; p < passes; p++)                                          \
        {                                                                            \
            for (size_t i = 0; i < w.count; i++)                                     \
            {                                                                        \
                const uint32_t* value = rg_hash_map_get_ptr_const(Type, &map, q[i]); \
                if (value != NULL) m.checksum += *value;                             \
            }                                                                        \
        }                                                                            \
        m.ms = get_time_ms() - start;                                                \
        return m;                                                                    \
    }                                                                                \
    static Measurement name##_find_ordered(const Workload& w, size_t passes) { return name##_find(w, passes, w.keys); }      \
    static Measurement name##_find_shuffled(const Workload& w, size_t passes) { return name##_find(w, passes, w.shuffled); } \
    static Measurement name##_find_missing(const Workload& w, size_t passes) { return name##_find(w, passes, w.missing); }   \
    static Measurement name##_remove(const Workload& w, size_t passes)               \
    {                                                                                \
        Measurement m = {0.0, 0};                                                    \
        for (size_t p = 0; p < passes; p++)                                          \
        {                                                                            \
            Type map = name##_build(w);                                              \
            uint64_t removed = 0;                                                   \
            double start = get_time_ms();                                           \
            for (size_t i = 0; i < w.count; i++)                                    \
            {                                                                       \
                int flag = 0;                                                       \
                rg_hash_map_remove(Type, &map, w.shuffled[i], &flag);               \
                removed += flag == 1;                                               \
            }                                                                       \
            m.ms += get_time_ms() - start;                                          \
            m.checksum += removed + rg_hash_map_count(&map);                        \
        }                                                                            \
        return m;                                                                    \
    }

RG_BENCH_MAP(rg, U32Map)
RG_BENCH_MAP(rg_mix, U32MixMap)

// =============================================================================
// std::unordered_map benchmarks
// =============================================================================

typedef std::unordered_map<uint32_t, uint32_t> StdMap;

static StdMap std_build(const Workload& w)
{
    StdMap map;
    map.max_load_factor(0.7f);
    map.reserve(w.count);
    for (size_t i = 0; i < w.count; i++)
    {
        map[w.keys[i]] = value_for(w.keys[i]);
    }
    return map;
}

static Measurement std_insert(const Workload& w, size_t passes)
{
    Measurement m = {0.0, 0};
    for (size_t p = 0; p < passes; p++)
    {
        StdMap map;
        map.max_load_factor(0.7f);
        map.reserve(w.count);
        uint64_t inserted = 0;
        double start = get_time_ms();
        for (size_t i = 0; i < w.count; i++)
        {
            inserted += map.emplace(w.keys[i], value_for(w.keys[i])).second;
        }
        m.ms += get_time_ms() - start;
        m.checksum += inserted + map.size();
    }
    return m;
}

static Measurement std_find(const Workload& w, size_t passes, const std::vector<uint32_t>& q)
{
    StdMap map = std_build(w);
    Measurement m = {0.0, 0};
    double start = get_time_ms();
    for (size_t p = 0; p < passes; p++)
    {
        for (size_t i = 0; i < w.count; i++)
        {
            StdMap::const_iterator it = map.find(q[i]);
            if (it != map.end())
            {
                m.checksum += it->second;
            }
        }
    }
    m.ms = get_time_ms() - start;
    return m;
}

static Measurement std_find_ordered(const Workload& w, size_t passes) { return std_find(w, passes, w.keys); }
static Measurement std_find_shuffled(const Workload& w, size_t passes) { return std_find(w, passes, w.shuffled); }
static Measurement std_find_missing(const Workload& w, size_t passes) { return std_find(w, passes, w.missing); }

static Measurement std_remove(const Workload& w, size_t passes)
{
    Measurement m = {0.0, 0};
    for (size_t p = 0; p < passes; p++)
    {
        StdMap map = std_build(w);
        uint64_t removed = 0;
        double start = get_time_ms();
        for (size_t i = 0; i < w.count; i++)
        {
            removed += map.erase(w.shuffled[i]);
        }
        m.ms += get_time_ms() - start;
        m.checksum += removed + map.size();
    }
    return m;
}

// =============================================================================
// stb_ds benchmarks
// =============================================================================

#ifdef RG_BENCH_STB_DS
typedef struct StbU32Pair
{
    uint32_t key;
    uint32_t value;
} StbU32Pair;

static StbU32Pair* stb_build(const Workload& w)
{
    StbU32Pair* map = NULL;
    for (size_t i = 0; i < w.count; i++)
    {
        hmput(map, w.keys[i], value_for(w.keys[i]));
    }
    return map;
}

static Measurement stb_insert(const Workload& w, size_t passes)
{
    Measurement m = {0.0, 0};
    for (size_t p = 0; p < passes; p++)
    {
        StbU32Pair* map = NULL;
        double start = get_time_ms();
        for (size_t i = 0; i < w.count; i++)
        {
            hmput(map, w.keys[i], value_for(w.keys[i]));
        }
        m.ms += get_time_ms() - start;
        /* hmput reports nothing per call; count the final entries twice so the
         * expected checksum matches the per-operation implementations. */
        m.checksum += 2u * static_cast<uint64_t>(hmlen(map));
        hmfree(map);
    }
    return m;
}

static Measurement stb_find(const Workload& w, size_t passes, const std::vector<uint32_t>& q)
{
    StbU32Pair* map = stb_build(w);
    Measurement m = {0.0, 0};
    double start = get_time_ms();
    for (size_t p = 0; p < passes; p++)
    {
        for (size_t i = 0; i < w.count; i++)
        {
            ptrdiff_t index = hmgeti(map, q[i]);
            if (index >= 0)
            {
                m.checksum += map[index].value;
            }
        }
    }
    m.ms = get_time_ms() - start;
    hmfree(map);
    return m;
}

static Measurement stb_find_ordered(const Workload& w, size_t passes) { return stb_find(w, passes, w.keys); }
static Measurement stb_find_shuffled(const Workload& w, size_t passes) { return stb_find(w, passes, w.shuffled); }
static Measurement stb_find_missing(const Workload& w, size_t passes) { return stb_find(w, passes, w.missing); }

static Measurement stb_remove(const Workload& w, size_t passes)
{
    Measurement m = {0.0, 0};
    for (size_t p = 0; p < passes; p++)
    {
        StbU32Pair* map = stb_build(w);
        uint64_t removed = 0;
        double start = get_time_ms();
        for (size_t i = 0; i < w.count; i++)
        {
            removed += hmdel(map, w.shuffled[i]) != 0;
        }
        m.ms += get_time_ms() - start;
        m.checksum += removed + static_cast<uint64_t>(hmlen(map));
        hmfree(map);
    }
    return m;
}
#endif

// =============================================================================
// Main
// =============================================================================

struct Case
{
    const char* name;
    CaseKind kind;
    HashBenchFn rg;
    HashBenchFn rg_mix; // the avalanche-hash map
    HashBenchFn std_fn;
#ifdef RG_BENCH_STB_DS
    HashBenchFn stb;
#endif
};

int main(void)
{
    int processor_index = pin_benchmark_thread();

    printf("rg_hash benchmarks\n");
    printf("==================\n\n");
    printf("Median of %d samples per implementation after calibration to at least %.0f ms per\n"
           "sample; implementation order rotates every sample; every pass is checksummed.\n",
           SAMPLE_COUNT, RG_BENCH_HASH_SAMPLE_MS);
    printf("Values are nanoseconds per operation; lower is better. Table construction is untimed.\n");
    printf("rg uses rg_hash_u32_fast; rg(mix) uses the rg_hash_u32 avalanche hash.\n");
    printf("std is std::unordered_map with max_load_factor 0.7 and reserve(count); its actual\n"
           "occupancy differs from rg's power-of-two table (for example 68.8%% vs 50%% at 4,096).\n\n");
    if (processor_index >= 0)
    {
        printf("Benchmark thread pinned to logical processor %d.\n\n", processor_index);
    }

    if (rg_malloc(MB(128)) != 0)
    {
        printf("Failed to initialize rg_malloc pool.\n");
        return 1;
    }
    g_arena = rg_arena_create(MB(64));
    if (g_arena.memory == NULL)
    {
        printf("Failed to create benchmark arena.\n");
        rg_free();
        return 1;
    }

    const Case cases[] = {
#ifdef RG_BENCH_STB_DS
        {"insert", CASE_INSERT, rg_insert, rg_mix_insert, std_insert, stb_insert},
        {"find, insertion order", CASE_FIND_HIT, rg_find_ordered, rg_mix_find_ordered, std_find_ordered, stb_find_ordered},
        {"find, shuffled order", CASE_FIND_HIT, rg_find_shuffled, rg_mix_find_shuffled, std_find_shuffled, stb_find_shuffled},
        {"find, missing keys", CASE_FIND_MISS, rg_find_missing, rg_mix_find_missing, std_find_missing, stb_find_missing},
        {"remove", CASE_REMOVE, rg_remove, rg_mix_remove, std_remove, stb_remove},
#else
        {"insert", CASE_INSERT, rg_insert, rg_mix_insert, std_insert},
        {"find, insertion order", CASE_FIND_HIT, rg_find_ordered, rg_mix_find_ordered, std_find_ordered},
        {"find, shuffled order", CASE_FIND_HIT, rg_find_shuffled, rg_mix_find_shuffled, std_find_shuffled},
        {"find, missing keys", CASE_FIND_MISS, rg_find_missing, rg_mix_find_missing, std_find_missing},
        {"remove", CASE_REMOVE, rg_remove, rg_mix_remove, std_remove},
#endif
    };
    const size_t sizes[] = {4096, 65536, 500000};

    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
    {
        Workload w = make_workload(sizes[s]);
        printf("%zu entries\n", w.count);
#ifdef RG_BENCH_STB_DS
        printf("  %-24s %9s %9s %9s %9s\n", "operation", "rg", "rg(mix)", "std", "stb_ds");
#else
        printf("  %-24s %9s %9s %9s\n", "operation", "rg", "rg(mix)", "std");
#endif
        for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++)
        {
            const Case& k = cases[c];
#ifdef RG_BENCH_STB_DS
            const Implementation impls[] = {{"rg", k.rg}, {"rg(mix)", k.rg_mix}, {"std", k.std_fn}, {"stb_ds", k.stb}};
#else
            const Implementation impls[] = {{"rg", k.rg}, {"rg(mix)", k.rg_mix}, {"std", k.std_fn}};
#endif
            const size_t impl_count = sizeof(impls) / sizeof(impls[0]);
            double ns[4] = {-1.0, -1.0, -1.0, -1.0};
            measure_case(k.name, k.kind, w, impls, impl_count, ns);
            printf("  %-24s", k.name);
            for (size_t i = 0; i < impl_count; i++)
            {
                if (ns[i] >= 0.0) printf(" %9.1f", ns[i]);
                else printf(" %9s", "-");
            }
            printf("\n");
        }
        printf("\n");
    }

    rg_arena_free(&g_arena);
    rg_free();
    if (g_validation_failures)
    {
        printf("%d validation failure(s).\n", g_validation_failures);
        return 1;
    }
    printf("All checksums matched.\n");
    return 0;
}
