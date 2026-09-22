// rg_sprintf public API correctness tests

#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif

#if defined(RG_SPRINTF_TEST_HYBRID)
#include "../src/rg_sprintf_hybrid.h"
#elif defined(RG_SPRINTF_TEST_ASM)
#include "../src/rg_sprintf_asm.h"
#else
#include "../src/rg_sprintf.h"
#endif

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

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

#define CHECK_FORMAT(expected, format, ...)                                \
	do                                                                     \
	{                                                                      \
		char actual_buffer[256];                                           \
		int actual_count = rg_sprintf(actual_buffer, format, __VA_ARGS__); \
		CHECK(strcmp(actual_buffer, expected) == 0);                       \
		CHECK(actual_count == (int)strlen(expected));                      \
	} while (0)

#define CHECK_LIBC(format, ...)                                                            \
	do                                                                                     \
	{                                                                                      \
		char expected_buffer[256];                                                         \
		char actual_buffer[256];                                                           \
		int expected_count = sprintf(expected_buffer, format, __VA_ARGS__);                \
		int actual_count = rg_sprintf(actual_buffer, format, __VA_ARGS__);                 \
		tests_run += 2;                                                                    \
		if (strcmp(actual_buffer, expected_buffer) != 0 || actual_count != expected_count) \
		{                                                                                  \
			printf("FAIL %s:%d: format %s\n", __FILE__, __LINE__, format);                 \
			printf("  libc: %s (%d)\n", expected_buffer, expected_count);                  \
			printf("  rg:   %s (%d)\n", actual_buffer, actual_count);                      \
			if (strcmp(actual_buffer, expected_buffer) != 0) tests_failed++;               \
			if (actual_count != expected_count) tests_failed++;                            \
		}                                                                                  \
	} while (0)

typedef struct CallbackBuffer
{
	char data[2048];
	size_t len;
	int calls;
} CallbackBuffer;

static void capture_callback(const char* buf, void* user, int len)
{
	CallbackBuffer* capture = (CallbackBuffer*)user;
	size_t available = sizeof(capture->data) - capture->len - 1;
	size_t copy_len = (size_t)len < available ? (size_t)len : available;

	memcpy(capture->data + capture->len, buf, copy_len);
	capture->len += copy_len;
	capture->data[capture->len] = '\0';
	capture->calls++;
}

static void test_basic_formatting(void)
{
	char buffer[64];
	int count = rg_sprintf(buffer, "plain text");

	CHECK(strcmp(buffer, "plain text") == 0);
	CHECK(count == 10);

	CHECK_FORMAT("value=-42", "value=%d", -42);
	CHECK_FORMAT("42", "%i", 42);
	CHECK_FORMAT("4294967295", "%u", UINT32_MAX);
	CHECK_FORMAT("deadbeef", "%x", 0xDEADBEEFu);
	CHECK_FORMAT("DEADBEEF", "%X", 0xDEADBEEFu);
	CHECK_FORMAT("755", "%o", 0755u);
	CHECK_FORMAT("100%", "%d%%", 100);
	CHECK_FORMAT("A", "%c", 'A');
	CHECK_FORMAT("hello", "%s", "hello");
}

static void test_width_precision_and_flags(void)
{
	CHECK_LIBC("%8d", 42);
	CHECK_LIBC("%-8d", 42);
	CHECK_LIBC("%08d", -42);
	CHECK_LIBC("%+d", 42);
	CHECK_LIBC("% d", 42);
	CHECK_LIBC("%.5d", 42);
	CHECK_LIBC("%#x", 0x2au);
	CHECK_LIBC("%#X", 0x2au);
	CHECK_LIBC("%#o", 0755u);
	CHECK_LIBC("%10s", "hello");
	CHECK_LIBC("%-10s", "hello");
	CHECK_LIBC("%.3s", "hello");
	CHECK_LIBC("%*.*f", 9, 3, 1.25);
}

static void check_string_output_modes(const char* format, ...)
{
	char expected[2048];
	char guarded[2050];
	CallbackBuffer capture = {{0}, 0, 0};
	va_list args;
	va_list copy;
	va_start(args, format);
	va_copy(copy, args);
	int expected_count = vsnprintf(expected, sizeof(expected), format, copy);
	va_end(copy);
	CHECK(expected_count >= 0 && (size_t)expected_count < sizeof(expected));
	if (expected_count < 0 || (size_t)expected_count >= sizeof(expected))
	{
		va_end(args);
		return;
	}

	memset(guarded, 'Z', sizeof(guarded));
	va_copy(copy, args);
	CHECK(rg_vsprintf(guarded + 1, format, copy) == expected_count);
	va_end(copy);
	CHECK(memcmp(guarded + 1, expected, (size_t)expected_count + 1) == 0);
	CHECK(guarded[0] == 'Z' && guarded[expected_count + 2] == 'Z');

	// Truncate in literals, leading padding, string data, and trailing padding.
	const size_t capacities[] = {0, 1, 7, 10, 11, 15, 16, 17, 32,
	                             (size_t)expected_count, (size_t)expected_count + 1};
	for (size_t i = 0; i < sizeof(capacities) / sizeof(capacities[0]); ++i)
	{
		size_t capacity = capacities[i];
		memset(guarded, 'Z', sizeof(guarded));
		va_copy(copy, args);
		CHECK(rg_vsnprintf(guarded + 1, capacity, format, copy) == expected_count);
		va_end(copy);
		CHECK(guarded[0] == 'Z' && guarded[capacity + 1] == 'Z');
		if (capacity != 0)
		{
			size_t copied = (size_t)expected_count < capacity - 1 ? (size_t)expected_count : capacity - 1;
			CHECK(memcmp(guarded + 1, expected, copied) == 0);
			CHECK(guarded[copied + 1] == '\0');
		}
	}
	va_copy(copy, args);
	CHECK(rg_vsnprintf(NULL, 0, format, copy) == expected_count);
	va_end(copy);
	va_copy(copy, args);
	CHECK(rg_vsprintf_cb(capture_callback, &capture, format, copy) == expected_count);
	va_end(copy);
	CHECK(capture.len == (size_t)expected_count);
	CHECK(memcmp(capture.data, expected, (size_t)expected_count + 1) == 0);
	if (expected_count > 0) CHECK(capture.calls > 0);
	va_end(args);
}

static void test_general_string_widths(void)
{
	static const size_t lengths[] = {0, 1, 7, 8, 9, 10, 11, 15, 16, 17, 31, 32, 33};
	static const int widths[] = {0, 7, 10, 16, 24, 40};
	static const int precisions[] = {0, 4, 9, 10, 15, 16, 17, 32};
	char text[34];
	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
	{
		for (size_t j = 0; j < lengths[i]; ++j) text[j] = (char)('a' + j % 26);
		text[lengths[i]] = '\0';
		int width = widths[i % (sizeof(widths) / sizeof(widths[0]))];
		int precision = precisions[i % (sizeof(precisions) / sizeof(precisions[0]))];
		CHECK_LIBC("%10s", text);
		CHECK_LIBC("%-10s", text);
		CHECK_LIBC("prefix[%24s]suffix", text);
		CHECK_LIBC("prefix[%-24s]suffix", text);
		CHECK_LIBC("%*s", width, text);
		CHECK_LIBC("%*s", -width, text);
		check_string_output_modes("label[%*s]tail", width, text);
		check_string_output_modes("label[%*s]tail", -width, text);
		check_string_output_modes("label[%*.*s]tail", width, precision, text);
		check_string_output_modes("label[%*.*s]tail", -width, precision, text);
	}

	// A short value can still cross callback chunks through its field padding.
	check_string_output_modes("begin[%*s]end", 513, "hero");
	check_string_output_modes("begin[%*s]end", -513, "hero");
	char payload[514];
	for (size_t length = 511; length <= 513; ++length)
	{
		memset(payload, 'q', length);
		payload[length] = '\0';
		check_string_output_modes("tag:%s:end", payload);
	}
}

static void test_integer_limits(void)
{
	CHECK_LIBC("%d", INT_MIN);
	CHECK_LIBC("%d", INT_MAX);
	CHECK_LIBC("%lld", (long long)INT64_MIN);
	CHECK_LIBC("%lld", (long long)INT64_MAX);
	CHECK_LIBC("%llu", (unsigned long long)UINT64_MAX);
	CHECK_LIBC("%zu", (size_t)1234567);
}

static void test_floating_point(void)
{
	CHECK_LIBC("%.2f", 3.25);
	CHECK_LIBC("%.3f", -1.125);
	CHECK_LIBC("%.6e", 0.000001);
	CHECK_LIBC("%.4g", 12.5);
	CHECK_LIBC("%+.1f", 2.0);
}

static void test_snprintf_bounds(void)
{
	char buffer[8];
	char one[1] = {'X'};
	int count;

	memset(buffer, 'X', sizeof(buffer));
	count = rg_snprintf(buffer, sizeof(buffer), "value=%d", 1234);
	CHECK(count == 10);
	CHECK(strcmp(buffer, "value=1") == 0);

	count = rg_snprintf(one, sizeof(one), "abc");
	CHECK(count == 3);
	CHECK(one[0] == '\0');

	count = rg_snprintf(NULL, 0, "value=%d", 1234);
	CHECK(count == 10);
}

static void check_string_slice(const char* slice)
{
	char buffer[16];
	CallbackBuffer capture = {{0}, 0, 0};
	CHECK_FORMAT("abcd", "%.4s", slice);
	CHECK_FORMAT("abcd", "%.*s", 4, slice);
	CHECK_LIBC("%*.*s", 8, 4, slice);
	CHECK_LIBC("%*.*s", -8, 4, slice);
	CHECK(rg_snprintf(buffer, 3, "%.*s", 4, slice) == 4);
	CHECK(strcmp(buffer, "ab") == 0);
	CHECK(rg_snprintf(NULL, 0, "%.*s", 4, slice) == 4);
	CHECK(rg_sprintf_cb(capture_callback, &capture, "prefix:%.4s:%d", slice, 42) == 14);
	CHECK(strcmp(capture.data, "prefix:abcd:42") == 0);
	CHECK(capture.len == 14);
}

static void test_string_precision(void)
{
	const char slice[4] = {'a', 'b', 'c', 'd'};
	const char embedded[4] = {'a', '\0', 'b', 'c'};
	check_string_slice(slice);
	CHECK_FORMAT("a", "%.*s", 4, embedded);
	CHECK_FORMAT("", "%.0s", slice);
	CHECK_LIBC("%.*s", -1, "complete");
	CHECK_LIBC("%.*s", 32, "short");
	CHECK_FORMAT("(nu", "%.3s", (char*)NULL);
}

static void test_string_precision_guard_page(void)
{
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
	size_t page_size;
	char* pages;
#if defined(_WIN32)
	SYSTEM_INFO info;
	DWORD old_protection;
	GetSystemInfo(&info);
	page_size = (size_t)info.dwPageSize;
	pages = (char*)VirtualAlloc(NULL, page_size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	CHECK(pages != NULL);
	if (pages == NULL) return;
	int protected_page = VirtualProtect(pages + page_size, page_size, PAGE_NOACCESS, &old_protection) != 0;
#else
	long system_page_size = sysconf(_SC_PAGESIZE);
	CHECK(system_page_size > 0);
	if (system_page_size <= 0) return;
	page_size = (size_t)system_page_size;
	pages = (char*)mmap(NULL, page_size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	CHECK(pages != MAP_FAILED);
	if (pages == MAP_FAILED) return;
	int protected_page = mprotect(pages + page_size, page_size, PROT_NONE) == 0;
#endif
	CHECK(protected_page);
	if (protected_page)
	{
		char* slice = pages + page_size - 4;
		memcpy(slice, "abcd", 4);
		check_string_slice(slice);
		// Zero precision must not access even the first source byte.
		CHECK_FORMAT("", "%.0s", pages + page_size);
		CHECK(rg_snprintf(NULL, 0, "%.*s", 0, pages + page_size) == 0);
	}
#if defined(_WIN32)
	CHECK(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
	CHECK(munmap(pages, page_size * 2) == 0);
#endif
#endif
}

static void test_direct_conversions(void)
{
	char buffer[64];
	char* end;

	end = rg_itoa(INT32_MIN, buffer);
	CHECK(strcmp(buffer, "-2147483648") == 0);
	CHECK(end == buffer + strlen(buffer));

	end = rg_utoa(UINT32_MAX, buffer);
	CHECK(strcmp(buffer, "4294967295") == 0);
	CHECK(end == buffer + strlen(buffer));

	end = rg_i64toa(INT64_MIN, buffer);
	CHECK(strcmp(buffer, "-9223372036854775808") == 0);
	CHECK(end == buffer + strlen(buffer));

	end = rg_u64toa(UINT64_MAX, buffer);
	CHECK(strcmp(buffer, "18446744073709551615") == 0);
	CHECK(end == buffer + strlen(buffer));

	end = rg_dtoa(12.5, buffer, 3);
	CHECK(strcmp(buffer, "12.500") == 0);
	CHECK(end == buffer + strlen(buffer));

	end = rg_ftoa(-0.25f, buffer, 2);
	CHECK(strcmp(buffer, "-0.25") == 0);
	CHECK(end == buffer + strlen(buffer));
}

static void check_u64_conversion(uint64_t value)
{
	char expected[21];
	int digits = sprintf(expected, "%llu", (unsigned long long)value);
	int paths = 1;
#if defined(RG_SPRINTF_HAS_ASM) && defined(RG_SPRINTF_ASM_U64TOA)
	paths = 2;
#endif
	for (int path = 0; path < paths; ++path)
	{
		char guarded[40];
		char* buffer = guarded + 8;
		char* end;
		int failures_before = tests_failed;
		int canaries_intact = 1;
		memset(guarded, 'Z', sizeof(guarded));
#if defined(RG_SPRINTF_HAS_ASM) && defined(RG_SPRINTF_ASM_U64TOA)
		// The public wrapper uses its C fast path for 19- and 20-digit values.
		if (path != 0)
			end = rg_u64toa_asm(value, buffer, digits, rg_digit_quads);
		else
#endif
			end = rg_u64toa(value, buffer);
		CHECK(end == buffer + digits);
		CHECK(memcmp(buffer, expected, (size_t)digits) == 0);
		CHECK(buffer[digits] == '\0');
		for (size_t i = 0; i < sizeof(guarded); ++i)
		{
			if ((i < 8 || i > (size_t)digits + 8) && guarded[i] != 'Z')
				canaries_intact = 0;
		}
		CHECK(canaries_intact);
		if (tests_failed != failures_before)
			printf("  u64 value=%llu path=%s\n", (unsigned long long)value,
			       path == 0 ? "public" : "asm");
	}
}

static void test_u64_conversion_boundaries(void)
{
	static const uint64_t chunk_quotients[] = {
		1, 2, 9, 10, 99, 100, 9999, 10000, 999999999,
		UINT64_C(1000000000), UINT64_C(1000000001),
		UINT64_C(18446744072), UINT64_C(18446744073)
	};
	static const uint32_t chunk_remainders[] = {
		0, 1, 9, 10, 99, 100, 9999, 10000, 99999999, 100000000, 999999999
	};
	const uint64_t chunk = UINT64_C(1000000000);
	uint64_t random_state = UINT64_C(0xD1B54A32D192ED03);

	check_u64_conversion(0);
	check_u64_conversion(UINT64_MAX);
	check_u64_conversion(UINT64_MAX - 1);
	check_u64_conversion(UINT64_MAX - 2);
	for (uint64_t power = 1;; power *= 10)
	{
		check_u64_conversion(power - 1);
		check_u64_conversion(power);
		check_u64_conversion(power + 1);
		if (power > UINT64_MAX / 10) break;
	}
	for (size_t i = 0; i < sizeof(chunk_quotients) / sizeof(chunk_quotients[0]); ++i)
	{
		uint64_t base = chunk_quotients[i] * chunk;
		check_u64_conversion(base - 1);
		for (size_t j = 0; j < sizeof(chunk_remainders) / sizeof(chunk_remainders[0]); ++j)
		{
			if (chunk_remainders[j] <= UINT64_MAX - base)
				check_u64_conversion(base + chunk_remainders[j]);
		}
	}
	for (int i = 0; i < 4096; ++i)
	{
		// Full-width deterministic inputs, plus exact quotient transitions.
		random_state ^= random_state >> 12;
		random_state ^= random_state << 25;
		random_state ^= random_state >> 27;
		uint64_t value = random_state * UINT64_C(2685821657736338717);
		check_u64_conversion(value);
		if (i < 1024)
		{
			uint64_t base = (value % (UINT64_MAX / chunk + 1)) * chunk;
			if (base != 0) check_u64_conversion(base - 1);
			check_u64_conversion(base);
			check_u64_conversion(base + 1);
		}
	}
}

static void test_hex_conversion(void)
{
	static const uint8_t input[] = {0x00, 0x12, 0xAB, 0xFF};
	uint8_t output[sizeof(input)] = {0};
	char hex[sizeof(input) * 2 + 1];
	char* end;
	int count;

	end = rg_to_hex(input, sizeof(input), hex, 0);
	CHECK(strcmp(hex, "0012abff") == 0);
	CHECK(end == hex + 8);

	rg_to_hex(input, sizeof(input), hex, 1);
	CHECK(strcmp(hex, "0012ABFF") == 0);

	count = rg_from_hex(hex, strlen(hex), output);
	CHECK(count == (int)sizeof(input));
	CHECK(memcmp(input, output, sizeof(input)) == 0);
	CHECK(rg_from_hex("abc", 3, output) == -1);
	CHECK(rg_from_hex("zz", 2, output) == -1);
}

static void test_callback_output(void)
{
	CallbackBuffer capture = {{0}, 0, 0};
	char payload[1500];
	int count;

	memset(payload, 'q', sizeof(payload) - 1);
	payload[sizeof(payload) - 1] = '\0';

	count = rg_sprintf_cb(capture_callback, &capture, "prefix:%s:%d", payload, 42);
	CHECK(count == 1509);
	CHECK(capture.len == 1509);
	CHECK(capture.calls >= 2);
	CHECK(strncmp(capture.data, "prefix:", 7) == 0);
	CHECK(strcmp(capture.data + capture.len - 3, ":42") == 0);
}

static void test_builder(void)
{
	static const uint8_t bytes[] = {0x12, 0xAB};
	char buffer[64];
	RgBuilder builder;
	size_t len;

	rg_builder_init(&builder, buffer, sizeof(buffer));
	rg_builder_append(&builder, "id=");
	rg_builder_append_uint(&builder, 42);
	rg_builder_append_char(&builder, ' ');
	rg_builder_appendf(&builder, "name=%s ", "hero");
	rg_builder_append_hex(&builder, bytes, sizeof(bytes));
	len = rg_builder_finish(&builder);

	CHECK(strcmp(buffer, "id=42 name=hero 12ab") == 0);
	CHECK(len == strlen(buffer));

	rg_builder_reset(&builder);
	rg_builder_append_float(&builder, 1.25, 2);
	CHECK(strcmp(buffer, "1.25") == 0);

	{
		char small[5];
		rg_builder_init(&builder, small, sizeof(small));
		rg_builder_append(&builder, "abcdef");
		CHECK(strcmp(small, "abcd") == 0);
		CHECK(rg_builder_finish(&builder) == 4);
	}
}

static void test_former_benchmark_templates(void)
{
	// These must remain correct through the generic formatter. Their exact
	// wording must not be recognized by special-case dispatch in the library.
	CHECK_LIBC("[frame %06u] dt=%.3fms fps=%u jobs=%u", 7u, 16.125, 60u, 12u);
	CHECK_LIBC("asset load %s/%s_%04u.rgi %.1f%%", "tiles", "jungle", 7u, 42.0);
	CHECK_LIBC("job[%02u] %s worker=%u time=%.3fms", 7u, "visibility", 3u, 0.020);
	CHECK_LIBC("warn %s:%d %s code=%08x", "renderer.c", 120, "resized", 0xC0000000u);
	CHECK_LIBC("net peer=%s seq=%u ack=%u ping=%d loss=%g", "east-2", 10u, 9u, 18, 0.25);
	CHECK_LIBC("prof %-10s %lld ticks %.6e", "physics", (long long)123456789, 0.000001);
	CHECK_LIBC("rect=(%d,%d %dx%d)", 32, 48, 96, 24);
	CHECK_LIBC("save slot=%u scene=%s version=%u bytes=%u", 1u, "dungeon", 42u, 1024u);
}

int main(void)
{
	test_basic_formatting();
	test_width_precision_and_flags();
	test_general_string_widths();
	test_integer_limits();
	test_floating_point();
	test_snprintf_bounds();
	test_string_precision();
	test_string_precision_guard_page();
	test_direct_conversions();
	test_u64_conversion_boundaries();
	test_hex_conversion();
	test_callback_output();
	test_builder();
	test_former_benchmark_templates();

	if (tests_failed != 0)
	{
		printf("%d of %d checks failed\n", tests_failed, tests_run);
		return 1;
	}

	printf("All %d rg_sprintf checks passed\n", tests_run);
	return 0;
}
