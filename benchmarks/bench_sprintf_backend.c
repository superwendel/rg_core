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

void BENCH_ENTRY(int argc, char** argv)
{
	rg_bench_init(argc, argv);
	bench_validate_only = argc > 1 && strcmp(argv[1], "--validate") == 0;
	for (int i = 0; i < BENCH_ROWS; ++i)
	{
		bench_data.integers[i] = (int)((uint32_t)i * UINT32_C(2654435761) & UINT32_C(0x7fffffff));
		bench_data.numbers[i] = 3.14159265358979 + (double)i * 0.001;
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
#undef RUN_SPRINTF_SAMPLE
	if (bench_validate_only) printf("Validated sprintf.%s outputs.\n", BENCH_BACKEND);
}
