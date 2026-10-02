// Linux x86-64 SysV ABI checks for the external integer formatting helpers.
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

char* rg_test_utoa_dirty_digits(uint32_t value, char* buf, int digits,
                               const char* digit_pairs, uint32_t upper_bits);
char* rg_test_u64toa_dirty_digits(uint64_t value, char* buf, int digits,
                                 const char* digit_quads, uint32_t upper_bits);

static char digit_pairs[201];
static char digit_quads[40001];
static int checks_run;
static int checks_failed;

static void check_conversion(uint64_t value, int wide, uint32_t upper_bits)
{
	char expected[21];
	char guarded[40];
	char* buffer = guarded + 8;
	int digits = snprintf(expected, sizeof(expected), "%" PRIu64, value);
	memset(guarded, 'Z', sizeof(guarded));
	char* end = wide
		? rg_test_u64toa_dirty_digits(value, buffer, digits, digit_quads, upper_bits)
		: rg_test_utoa_dirty_digits((uint32_t)value, buffer, digits, digit_pairs, upper_bits);
	int canaries_intact = 1;
	for (size_t i = 0; i < sizeof(guarded); ++i)
	{
		if ((i < 8 || i > (size_t)digits + 8) && guarded[i] != 'Z')
			canaries_intact = 0;
	}
	checks_run += 3;
	if (end != buffer + digits ||
	    memcmp(buffer, expected, (size_t)digits + 1) != 0 || !canaries_intact)
	{
		checks_failed++;
		printf("FAIL %s value=%" PRIu64 " upper_bits=%08" PRIx32 "\n",
		       wide ? "u64" : "u32", value, upper_bits);
	}
}

int main(void)
{
	static const uint32_t upper_bits[] = {
		0, 1, UINT32_C(0x80000000), UINT32_MAX
	};
	static const uint32_t values32[] = {
		0, 1, 9, 10, 99, 100, 9999, 10000, 9999999, 10000000,
		INT32_MAX, UINT32_MAX
	};
	static const uint64_t values64[] = {
		0, 1, 9, 10, 99, 100, 9999, 10000, UINT32_MAX,
		UINT64_C(999999999), UINT64_C(1000000000),
		UINT64_C(999999999999999999), UINT64_C(1000000000000000000),
		INT64_MAX, UINT64_MAX
	};

	for (unsigned i = 0; i < 100; ++i)
		(void)snprintf(digit_pairs + i * 2, 3, "%02u", i);
	for (unsigned i = 0; i < 10000; ++i)
		(void)snprintf(digit_quads + i * 4, 5, "%04u", i);

	for (size_t i = 0; i < sizeof(upper_bits) / sizeof(upper_bits[0]); ++i)
	{
		for (size_t j = 0; j < sizeof(values32) / sizeof(values32[0]); ++j)
			check_conversion(values32[j], 0, upper_bits[i]);
		for (size_t j = 0; j < sizeof(values64) / sizeof(values64[0]); ++j)
			check_conversion(values64[j], 1, upper_bits[i]);
	}

	if (checks_failed != 0)
	{
		printf("%d ABI conversions failed\n", checks_failed);
		return 1;
	}
	printf("All %d rg_sprintf SysV ABI checks passed\n", checks_run);
	return 0;
}
