// Compile this source once per backend. Include paths select either current
// headers or a baseline snapshot.
#include "bench_common.h"
#include <stdlib.h>

#if defined(RG_BENCH_SPRINTF_STB)
#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>
#define BENCH_FORMAT stbsp_sprintf
#define BENCH_BOUNDED stbsp_snprintf
#define BENCH_BACKEND "stb"
#define BENCH_ENTRY rg_bench_sprintf_stb
#elif defined(RG_BENCH_SPRINTF_ASM)
#include <rg_sprintf_asm.h>
#define BENCH_FORMAT rg_sprintf
#define BENCH_BOUNDED rg_snprintf
#define BENCH_BACKEND "asm"
#define BENCH_ENTRY rg_bench_sprintf_asm
#else
#include <rg_sprintf.h>
#define BENCH_FORMAT rg_sprintf
#define BENCH_BOUNDED rg_snprintf
#define BENCH_BACKEND "c"
#define BENCH_ENTRY rg_bench_sprintf_c
#endif

enum { BENCH_ROWS = 4096, BENCH_BLOCKS = 64, BENCH_OUTPUT_SIZE = 128 };

typedef struct SprintfBenchData
{
	char output[BENCH_ROWS][BENCH_OUTPUT_SIZE];
	int counts[BENCH_ROWS];
	char expected[BENCH_ROWS][BENCH_OUTPUT_SIZE];
	int expected_counts[BENCH_ROWS];
	int integers[BENCH_ROWS];
	double numbers[BENCH_ROWS];
	char long_text[4097];
	// Append new inputs so existing cases retain their data field offsets.
	char string_text[16][34];
	int widths[BENCH_ROWS];
	int signed_widths[BENCH_ROWS];
	int precisions[BENCH_ROWS];
#if !defined(RG_BENCH_SPRINTF_STB)
	double zero_numbers[BENCH_ROWS];
	int dtoa_precisions[BENCH_ROWS];
	RgBuilder builders[BENCH_ROWS];
#endif
	uint64_t u64_values[11][BENCH_ROWS];
	uint64_t unsigned64_values[BENCH_ROWS];
	int64_t signed64_values[BENCH_ROWS];
} SprintfBenchData;

static SprintfBenchData bench_data;
static int bench_validate_only;
static const char* const bench_names[] = {
	"Hello, world!", "Frame update", "Physics tick", "Audio mixer"
};
static const char* const bench_short_names[] = { "", "a", "ab", "abcdefghi" };

static void validate_output(const SprintfBenchData* data, const char* name)
{
	for (int i = 0; i < BENCH_ROWS; ++i)
	{
		size_t bytes = strlen(data->expected[i]) + 1;
		if (data->counts[i] != data->expected_counts[i] ||
		    memcmp(data->output[i], data->expected[i], bytes) != 0)
		{
			fprintf(stderr, "%s.%s output mismatch at row %d\n", BENCH_BACKEND, name, i);
			exit(1);
		}
	}
}

// Zero capacity selects sprintf; positive capacity selects snprintf. Each case
// uses rotating prebuilt inputs. Validate and
// consume all outputs and counts outside timing. The sink stays opaque to LTCG.
#define DEFINE_SPRINTF_SAMPLE(name, capacity, format, ...)                            \
	static double sample_##name(void* context, uint64_t* checksum)                    \
	{                                                                                \
		SprintfBenchData* data = (SprintfBenchData*)context;                           \
		double elapsed = 0.0;                                                        \
		uint64_t sum = 0;                                                            \
		memset(data->output, 0, sizeof(data->output));                                 \
		for (int i = 0; i < BENCH_ROWS; ++i)                                          \
		{                                                                            \
			data->expected_counts[i] = snprintf(data->expected[i],                    \
			    (capacity) ? (capacity) : BENCH_OUTPUT_SIZE, format, __VA_ARGS__);    \
		}                                                                            \
		for (int block = 0; block < (bench_validate_only ? 1 : BENCH_BLOCKS); ++block) \
		{                                                                            \
			double start = bench_validate_only ? 0.0 : rg_bench_now_ns();              \
			for (int i = 0; i < BENCH_ROWS; ++i)                                      \
			{                                                                        \
				char* out = data->output[i];                                         \
				data->counts[i] = (capacity)                                         \
				    ? BENCH_BOUNDED(out, capacity, format, __VA_ARGS__)              \
				    : BENCH_FORMAT(out, format, __VA_ARGS__);                        \
			}                                                                        \
			if (!bench_validate_only) elapsed += rg_bench_now_ns() - start;           \
			validate_output(data, #name);                                            \
			sum += rg_bench_consume(data->output, sizeof(data->output));               \
			sum += rg_bench_consume(data->counts, sizeof(data->counts));               \
		}                                                                            \
		*checksum = sum;                                                             \
		return elapsed / ((double)BENCH_ROWS * BENCH_BLOCKS);                         \
	}

DEFINE_SPRINTF_SAMPLE(integer, 0, "%d", data->integers[i])
DEFINE_SPRINTF_SAMPLE(uint_padded, 0, "%08u", (unsigned)data->integers[i])
DEFINE_SPRINTF_SAMPLE(hex, 0, "%x", (unsigned)data->integers[i])
DEFINE_SPRINTF_SAMPLE(int64, 0, "%lld", 1234567890123LL + data->integers[i])
DEFINE_SPRINTF_SAMPLE(fixed, 0, "%.6f", data->numbers[i])
DEFINE_SPRINTF_SAMPLE(exponent, 0, "%.6e", data->numbers[i])
DEFINE_SPRINTF_SAMPLE(general, 0, "%g", data->numbers[i] * 0.000001)
DEFINE_SPRINTF_SAMPLE(string, 0, "%s", bench_names[i & 3])
DEFINE_SPRINTF_SAMPLE(string_width, 0, "%10s", bench_names[i & 3])
DEFINE_SPRINTF_SAMPLE(string_left, 0, "%-10s", bench_names[i & 3])
DEFINE_SPRINTF_SAMPLE(mixed, 0, "Player %s: HP=%d/%d Pos=(%.2f, %.2f)",
	bench_names[i & 3], 85 + (i % 15), 100, data->numbers[i], 678.90 - data->numbers[i])
DEFINE_SPRINTF_SAMPLE(bounded_string, BENCH_OUTPUT_SIZE, "name=%s", bench_names[i & 3])
DEFINE_SPRINTF_SAMPLE(slice_short, 0, "%.*s", 4, bench_names[i & 3])
DEFINE_SPRINTF_SAMPLE(slice_large_precision, 0, "%.*s", 4096, bench_names[i & 3])
DEFINE_SPRINTF_SAMPLE(slice_long, 0, "%.*s", 16, data->long_text + (i & 15))
DEFINE_SPRINTF_SAMPLE(slice_truncated, 8, "%.*s", 16, data->long_text + (i & 15))

// General formats and strings on both sides of short-copy/field boundaries.
// Keep the original cases above unchanged for paired historical comparisons.
DEFINE_SPRINTF_SAMPLE(string_lengths, 0, "%s", data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(string_dynamic_width, 0, "%*s", data->widths[i], data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(string_dynamic_left, 0, "%*s", -data->widths[i], data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(string_width_affixes, 0, "label=[%24s] end", data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(string_left_affixes, 0, "label=[%-24s] end", data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(string_width_precision, 0, "[%*.*s]:end",
	data->signed_widths[i], data->precisions[i], data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(string_width_bounded, BENCH_OUTPUT_SIZE, "name=[%*s]:end",
	data->signed_widths[i], data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(string_width_truncated, 13, "name=[%*s]:end",
	data->signed_widths[i], data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(string_width_lengths, 0, "%10s",
	(i & 16) ? bench_short_names[i & 3] : data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(string_left_lengths, 0, "%-10s",
	(i & 16) ? bench_short_names[i & 3] : data->string_text[i & 15])
DEFINE_SPRINTF_SAMPLE(int64_mixed, 0, "%lld", (long long)data->signed64_values[i])
DEFINE_SPRINTF_SAMPLE(uint64_mixed, 0, "%llu", (unsigned long long)data->unsigned64_values[i])
DEFINE_SPRINTF_SAMPLE(uint64_truncated, 8, "%llu", (unsigned long long)data->unsigned64_values[i])

#if !defined(RG_BENCH_SPRINTF_STB)
static double sample_u64toa_values(SprintfBenchData* data, uint64_t* checksum,
                                   const uint64_t* values, const char* name)
{
	double elapsed = 0.0;
	uint64_t sum = 0;
	memset(data->output, 0, sizeof(data->output));
	for (int i = 0; i < BENCH_ROWS; ++i)
		data->expected_counts[i] = snprintf(data->expected[i], BENCH_OUTPUT_SIZE,
		                                   "%llu", (unsigned long long)values[i]);
	for (int block = 0; block < (bench_validate_only ? 1 : BENCH_BLOCKS); ++block)
	{
		double start = bench_validate_only ? 0.0 : rg_bench_now_ns();
		for (int i = 0; i < BENCH_ROWS; ++i)
		{
			char* end = rg_u64toa(values[i], data->output[i]);
			data->counts[i] = (int)(end - data->output[i]);
		}
		if (!bench_validate_only) elapsed += rg_bench_now_ns() - start;
		validate_output(data, name);
		sum += rg_bench_consume(data->output, sizeof(data->output));
		sum += rg_bench_consume(data->counts, sizeof(data->counts));
	}
	*checksum = sum;
	return elapsed / ((double)BENCH_ROWS * BENCH_BLOCKS);
}

#define DEFINE_U64TOA_SAMPLE(digits)                                                   \
	static double sample_u64toa_##digits(void* context, uint64_t* checksum)            \
	{                                                                                \
		SprintfBenchData* data = (SprintfBenchData*)context;                           \
		return sample_u64toa_values(data, checksum, data->u64_values[(digits) - 10],   \
		                            "u64toa_" #digits);                              \
	}

DEFINE_U64TOA_SAMPLE(10)
DEFINE_U64TOA_SAMPLE(11)
DEFINE_U64TOA_SAMPLE(12)
DEFINE_U64TOA_SAMPLE(13)
DEFINE_U64TOA_SAMPLE(14)
DEFINE_U64TOA_SAMPLE(15)
DEFINE_U64TOA_SAMPLE(16)
DEFINE_U64TOA_SAMPLE(17)
DEFINE_U64TOA_SAMPLE(18)
DEFINE_U64TOA_SAMPLE(19)
DEFINE_U64TOA_SAMPLE(20)
#undef DEFINE_U64TOA_SAMPLE

static double sample_dtoa_values(SprintfBenchData* data, uint64_t* checksum,
                                 const double* values, const char* name)
{
	double elapsed = 0.0;
	uint64_t sum = 0;
	memset(data->output, 0, sizeof(data->output));
	for (int i = 0; i < BENCH_ROWS; ++i)
		data->expected_counts[i] = snprintf(data->expected[i], BENCH_OUTPUT_SIZE,
		                                   "%.*f", data->dtoa_precisions[i], values[i]);
	for (int block = 0; block < (bench_validate_only ? 1 : BENCH_BLOCKS); ++block)
	{
		double start = bench_validate_only ? 0.0 : rg_bench_now_ns();
		for (int i = 0; i < BENCH_ROWS; ++i)
		{
			char* end = rg_dtoa(values[i], data->output[i], data->dtoa_precisions[i]);
			data->counts[i] = (int)(end - data->output[i]);
		}
		if (!bench_validate_only) elapsed += rg_bench_now_ns() - start;
		validate_output(data, name);
		sum += rg_bench_consume(data->output, sizeof(data->output));
		sum += rg_bench_consume(data->counts, sizeof(data->counts));
	}
	*checksum = sum;
	return elapsed / ((double)BENCH_ROWS * BENCH_BLOCKS);
}

static double sample_dtoa_nonzero(void* context, uint64_t* checksum)
{
	SprintfBenchData* data = (SprintfBenchData*)context;
	return sample_dtoa_values(data, checksum, data->numbers, "dtoa_nonzero");
}

static double sample_dtoa_zero(void* context, uint64_t* checksum)
{
	SprintfBenchData* data = (SprintfBenchData*)context;
	return sample_dtoa_values(data, checksum, data->zero_numbers, "dtoa_zero");
}

static double sample_builder_format(SprintfBenchData* data, uint64_t* checksum,
                                    size_t capacity, const char* name)
{
	double elapsed = 0.0;
	uint64_t sum = 0;
	memset(data->output, 0, sizeof(data->output));
	for (int i = 0; i < BENCH_ROWS; ++i)
	{
		const char* prefix = bench_short_names[1 + i % 3];
		(void)snprintf(data->expected[i], capacity, "%sname=%s id=%d",
		               prefix, bench_names[i & 3], data->integers[i]);
		data->expected_counts[i] = (int)strlen(data->expected[i]);
	}
	for (int block = 0; block < (bench_validate_only ? 1 : BENCH_BLOCKS); ++block)
	{
		// Reset and append the existing prefix outside the measured operation.
		for (int i = 0; i < BENCH_ROWS; ++i)
		{
			rg_builder_init(&data->builders[i], data->output[i], capacity);
			rg_builder_append(&data->builders[i], bench_short_names[1 + i % 3]);
		}
		double start = bench_validate_only ? 0.0 : rg_bench_now_ns();
		for (int i = 0; i < BENCH_ROWS; ++i)
		{
			rg_builder_appendf(&data->builders[i], "name=%s id=%d",
			                   bench_names[i & 3], data->integers[i]);
			data->counts[i] = (int)data->builders[i].len;
		}
		if (!bench_validate_only) elapsed += rg_bench_now_ns() - start;
		validate_output(data, name);
		sum += rg_bench_consume(data->output, sizeof(data->output));
		sum += rg_bench_consume(data->counts, sizeof(data->counts));
	}
	*checksum = sum;
	return elapsed / ((double)BENCH_ROWS * BENCH_BLOCKS);
}

static double sample_builder_appendf(void* context, uint64_t* checksum)
{
	return sample_builder_format((SprintfBenchData*)context, checksum,
	                             BENCH_OUTPUT_SIZE, "builder_appendf");
}

static double sample_builder_appendf_truncated(void* context, uint64_t* checksum)
{
	return sample_builder_format((SprintfBenchData*)context, checksum,
	                             17, "builder_appendf_truncated");
}
#endif

void BENCH_ENTRY(int argc, char** argv)
{
	rg_bench_init(argc, argv);
	bench_validate_only = argc > 1 && strcmp(argv[1], "--validate") == 0;
	for (int i = 0; i < BENCH_ROWS; ++i)
	{
		bench_data.integers[i] = (int)((uint32_t)i * UINT32_C(2654435761) & UINT32_C(0x7fffffff));
		bench_data.numbers[i] = 3.14159265358979 + (double)i * 0.001;
#if !defined(RG_BENCH_SPRINTF_STB)
		bench_data.zero_numbers[i] = (i & 1) ? -0.0 : 0.0;
		bench_data.dtoa_precisions[i] = ((i / 17) & 3) * 2;
#endif
	}
	for (size_t i = 0; i < sizeof(bench_data.long_text) - 1; ++i)
		bench_data.long_text[i] = (char)('a' + i % 26);
	bench_data.long_text[sizeof(bench_data.long_text) - 1] = '\0';
	{
		static const unsigned lengths[] = {0, 1, 7, 8, 9, 10, 11, 15, 16, 17, 23, 24, 25, 31, 32, 33};
		static const int widths[] = {0, 7, 10, 16, 24, 40};
		static const int precisions[] = {0, 4, 9, 10, 15, 16, 17, 32};
		for (unsigned i = 0; i < 16; ++i)
		{
			for (unsigned j = 0; j < lengths[i]; ++j)
				bench_data.string_text[i][j] = (char)('a' + (j + 3 * i) % 26);
			bench_data.string_text[i][lengths[i]] = '\0';
		}
		for (int i = 0; i < BENCH_ROWS; ++i)
		{
			bench_data.widths[i] = widths[(i / 32) % 6];
			bench_data.signed_widths[i] = (i & 16) ? -bench_data.widths[i] : bench_data.widths[i];
			bench_data.precisions[i] = precisions[(i / 192) % 8];
		}
	}
	{
		uint64_t lower = UINT64_C(1000000000);
		for (int digits = 10; digits <= 20; ++digits)
		{
			uint64_t upper = digits == 20 ? UINT64_MAX : lower * 10u - 1u;
			uint64_t span = upper - lower + 1u;
			for (int i = 0; i < BENCH_ROWS; ++i)
			{
				// Mix before reducing so every length spans its full numeric range.
				uint64_t value = (uint64_t)(i + 1) * UINT64_C(0x9e3779b97f4a7c15);
				value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
				value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
				value ^= value >> 31;
				value = lower + value % span;
				// Include both sides of every decimal boundary across cases.
				switch (i & 31)
				{
				case 0: value = lower; break;
				case 1: value = lower + 1u; break;
				case 2: value = upper - 1u; break;
				case 3: value = upper; break;
				}
				bench_data.u64_values[digits - 10][i] = value;
			}
			if (digits != 20) lower *= 10u;
		}
		for (int i = 0; i < BENCH_ROWS; ++i)
		{
			uint64_t value = bench_data.u64_values[i % 11][i];
			switch (i % 23)
			{
			case 0: value = 0; break;
			case 1: value = 1; break;
			case 2: value = 9; break;
			case 3: value = 10; break;
			case 4: value = UINT32_MAX; break;
			case 5: value = (uint64_t)UINT32_MAX + 1u; break;
			case 6: value = INT64_MAX; break;
			case 7: value = (uint64_t)INT64_MAX + 1u; break;
			case 8: value = UINT64_MAX; break;
			}
			bench_data.unsigned64_values[i] = value;
			int64_t magnitude = (int64_t)(value & (uint64_t)INT64_MAX);
			bench_data.signed64_values[i] = (i & 1) ? -magnitude : magnitude;
			if (i % 29 == 0) bench_data.signed64_values[i] = INT64_MIN;
		}
	}

#define RUN_SPRINTF_SAMPLE(name)                                                      \
	do                                                                               \
	{                                                                                \
		if (bench_validate_only)                                                     \
		{                                                                            \
			uint64_t checksum;                                                      \
			(void)sample_##name(&bench_data, &checksum);                              \
		}                                                                            \
		else                                                                         \
			rg_bench_run("sprintf." BENCH_BACKEND "." #name, sample_##name, &bench_data); \
	} while (0)
	RUN_SPRINTF_SAMPLE(integer);
	RUN_SPRINTF_SAMPLE(uint_padded);
	RUN_SPRINTF_SAMPLE(hex);
	RUN_SPRINTF_SAMPLE(int64);
	RUN_SPRINTF_SAMPLE(fixed);
	RUN_SPRINTF_SAMPLE(exponent);
	RUN_SPRINTF_SAMPLE(general);
	RUN_SPRINTF_SAMPLE(string);
	RUN_SPRINTF_SAMPLE(string_width);
	RUN_SPRINTF_SAMPLE(string_left);
	RUN_SPRINTF_SAMPLE(mixed);
	RUN_SPRINTF_SAMPLE(bounded_string);
	RUN_SPRINTF_SAMPLE(slice_short);
	RUN_SPRINTF_SAMPLE(slice_large_precision);
	RUN_SPRINTF_SAMPLE(slice_long);
	RUN_SPRINTF_SAMPLE(slice_truncated);
	RUN_SPRINTF_SAMPLE(string_lengths);
	RUN_SPRINTF_SAMPLE(string_dynamic_width);
	RUN_SPRINTF_SAMPLE(string_dynamic_left);
	RUN_SPRINTF_SAMPLE(string_width_affixes);
	RUN_SPRINTF_SAMPLE(string_left_affixes);
	RUN_SPRINTF_SAMPLE(string_width_precision);
	RUN_SPRINTF_SAMPLE(string_width_bounded);
	RUN_SPRINTF_SAMPLE(string_width_truncated);
	RUN_SPRINTF_SAMPLE(string_width_lengths);
	RUN_SPRINTF_SAMPLE(string_left_lengths);
	RUN_SPRINTF_SAMPLE(int64_mixed);
	RUN_SPRINTF_SAMPLE(uint64_mixed);
	RUN_SPRINTF_SAMPLE(uint64_truncated);
#if !defined(RG_BENCH_SPRINTF_STB)
	RUN_SPRINTF_SAMPLE(u64toa_10);
	RUN_SPRINTF_SAMPLE(u64toa_11);
	RUN_SPRINTF_SAMPLE(u64toa_12);
	RUN_SPRINTF_SAMPLE(u64toa_13);
	RUN_SPRINTF_SAMPLE(u64toa_14);
	RUN_SPRINTF_SAMPLE(u64toa_15);
	RUN_SPRINTF_SAMPLE(u64toa_16);
	RUN_SPRINTF_SAMPLE(u64toa_17);
	RUN_SPRINTF_SAMPLE(u64toa_18);
	RUN_SPRINTF_SAMPLE(u64toa_19);
	RUN_SPRINTF_SAMPLE(u64toa_20);
	RUN_SPRINTF_SAMPLE(dtoa_nonzero);
	RUN_SPRINTF_SAMPLE(dtoa_zero);
	RUN_SPRINTF_SAMPLE(builder_appendf);
	RUN_SPRINTF_SAMPLE(builder_appendf_truncated);
#endif
#undef RUN_SPRINTF_SAMPLE
	if (bench_validate_only) printf("Validated sprintf.%s outputs.\n", BENCH_BACKEND);
}
