// rg_sprintf - Fast, portable printf-style formatting for C
//
// Part of the Reverse Gravity (rg_) core libraries.
// Single-header sprintf-style formatter.
//
// USAGE:
//   #include "rg_sprintf.h"
//   // All functions are static - just include and use
//
// OPTIONS:
//   #define RG_SPRINTF_NO_SIMD     - Disable SIMD optimizations
//   #define RG_SPRINTF_ROUND_TIES_AWAY 1 - Round exact float ties away from zero like the
//                                    Microsoft CRT (default: half to even, like glibc/musl/macOS)
//   #define RG_SPRINTF_SECURE      - Enable security checks (null checks, width/precision limits)
//   #define RG_SPRINTF_ASSERT(x)   - Custom assert macro
//
// Author: Steven Wendel (superwendel)

// The first formatter implementation included owns this translation unit.
#if !defined(RG_SPRINTF_H) && !defined(RG_SPRINTF_ASM_H)
#define RG_SPRINTF_H

#include "rg_defs.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include <math.h>

// All functions are static for unity build compatibility

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Format string to buffer (like sprintf)
 * @param buf Destination buffer
 * @param fmt Printf-style format string
 * @return Number of characters written (excluding null terminator)
 */
static int rg_sprintf(char* buf, const char* fmt, ...);

/**
 * @brief Format string to buffer with size limit (like snprintf)
 * @param buf Destination buffer
 * @param count Maximum number of characters to write (including null)
 * @param fmt Printf-style format string
 * @return Number of characters that would have been written (excluding null)
 */
static int rg_snprintf(char* buf, size_t count, const char* fmt, ...);

/**
 * @brief Format string to buffer using va_list
 * @param buf Destination buffer
 * @param fmt Printf-style format string
 * @param args Variable argument list
 * @return Number of characters written (excluding null terminator)
 */
static int rg_vsprintf(char* buf, const char* fmt, va_list args);

/**
 * @brief Format string to buffer with size limit using va_list
 * @param buf Destination buffer
 * @param count Maximum number of characters to write (including null)
 * @param fmt Printf-style format string
 * @param args Variable argument list
 * @return Number of characters that would have been written (excluding null)
 */
static int rg_vsnprintf(char* buf, size_t count, const char* fmt, va_list args);

/** @brief Callback type for custom output chunks (files, network, etc.) */
typedef void (*rg_sprintf_callback)(const char* buf, void* user, int len);

/**
 * @brief Format string with callback for custom output
 * @param cb Callback function called with formatted chunks
 * @param user User data passed to callback
 * @param fmt Printf-style format string
 * @return Number of characters written
 */
static int rg_sprintf_cb(rg_sprintf_callback cb, void* user, const char* fmt, ...);

/**
 * @brief Format string with callback using va_list
 * @param cb Callback function called with formatted chunks
 * @param user User data passed to callback
 * @param fmt Printf-style format string
 * @param args Variable argument list
 * @return Number of characters written
 */
static int rg_vsprintf_cb(rg_sprintf_callback cb, void* user, const char* fmt, va_list args);

/**
 * @brief Fast signed 32-bit integer to string
 * @param value Integer value to convert
 * @param buf Destination buffer (must hold at least 12 bytes)
 * @return Pointer to end of string (one past last char)
 */
RGINLINE char* rg_itoa(int32_t value, char* buf);

/**
 * @brief Fast unsigned 32-bit integer to string
 * @param value Integer value to convert
 * @param buf Destination buffer (must hold at least 11 bytes)
 * @return Pointer to end of string (one past last char)
 */
RGINLINE char* rg_utoa(uint32_t value, char* buf);

/**
 * @brief Fast signed 64-bit integer to string
 * @param value Integer value to convert
 * @param buf Destination buffer (must hold at least 21 bytes)
 * @return Pointer to end of string (one past last char)
 */
RGINLINE char* rg_i64toa(int64_t value, char* buf);

/**
 * @brief Fast unsigned 64-bit integer to string
 * @param value Integer value to convert
 * @param buf Destination buffer (must hold at least 21 bytes)
 * @return Pointer to end of string (one past last char)
 */
RGINLINE char* rg_u64toa(uint64_t value, char* buf);

/**
 * @brief Convert binary data to hex string
 * @param data Source data
 * @param len Number of bytes to convert
 * @param out Output buffer (must hold at least len*2+1 bytes)
 * @param uppercase Use uppercase hex digits (A-F) if non-zero
 * @return Pointer to end of string (null terminator position)
 */
RGINLINE char* rg_to_hex(const void* data, size_t len, char* out, int uppercase);

/**
 * @brief Convert hex string to binary data
 * @param hex Hex string (must be even length)
 * @param hex_len Length of hex string
 * @param out Output buffer (must hold at least hex_len/2 bytes)
 * @return Number of bytes written, or -1 on error
 */
RGINLINE int rg_from_hex(const char* hex, size_t hex_len, void* out);

/**
 * @brief Float to string in %f style, correctly rounded (converts through rg_dtoa)
 * @param value Float value to convert
 * @param buf Destination buffer of at least RG_SPRINTF_FLOAT_BUFFER_SIZE bytes,
 *            or 24 + precision bytes when |value| < 1e18
 * @param precision Number of decimal places (negative selects 6; clamped to
 *                  RG_SPRINTF_FLOAT_MAX_PRECISION)
 * @return Pointer to the terminating null byte
 */
RGINLINE char* rg_ftoa(float value, char* buf, int precision);

/**
 * @brief Double to string in %f style, correctly rounded
 *
 * Every digit is exact and ties round to even (RG_SPRINTF_ROUND_TIES_AWAY=1
 * rounds them away from zero), so the output matches glibc, musl and macOS
 * printf("%.*f") bit for bit. Infinity and NaN print as "inf" and "nan".
 * @param value Double value to convert
 * @param buf Destination buffer of at least RG_SPRINTF_FLOAT_BUFFER_SIZE bytes,
 *            or 24 + precision bytes when |value| < 1e18
 * @param precision Number of decimal places (negative selects 6; clamped to
 *                  RG_SPRINTF_FLOAT_MAX_PRECISION)
 * @return Pointer to the terminating null byte
 */
RGINLINE char* rg_dtoa(double value, char* buf, int precision);

/** @brief String builder for efficient concatenation */
typedef struct
{
	char* buf;
	size_t len;
	size_t cap;
} RgBuilder;

/**
 * @brief Initialize a string builder
 * @param b Builder to initialize
 * @param buf Buffer to use for building
 * @param cap Capacity of buffer
 */
RGINLINE void rg_builder_init(RgBuilder* b, char* buf, size_t cap);

/**
 * @brief Append a null-terminated string
 * @param b Builder
 * @param str String to append
 */
RGINLINE void rg_builder_append(RgBuilder* b, const char* str);

/**
 * @brief Append a string with explicit length
 * @param b Builder
 * @param str String to append
 * @param n Number of characters to append
 */
RGINLINE void rg_builder_appendn(RgBuilder* b, const char* str, size_t n);

/**
 * @brief Append formatted string (printf-style)
 * @param b Builder
 * @param fmt Format string
 */
static void rg_builder_appendf(RgBuilder* b, const char* fmt, ...);

/**
 * @brief Append a single character
 * @param b Builder
 * @param c Character to append
 */
RGINLINE void rg_builder_append_char(RgBuilder* b, char c);

/**
 * @brief Append a signed 64-bit integer
 * @param b Builder
 * @param value Integer value
 */
RGINLINE void rg_builder_append_int(RgBuilder* b, int64_t value);

/**
 * @brief Append an unsigned 64-bit integer
 * @param b Builder
 * @param value Integer value
 */
RGINLINE void rg_builder_append_uint(RgBuilder* b, uint64_t value);

/**
 * @brief Append binary data as lowercase hex
 * @param b Builder
 * @param data Source data
 * @param len Number of bytes to convert
 */
RGINLINE void rg_builder_append_hex(RgBuilder* b, const void* data, size_t len);

/**
 * @brief Append a floating-point number
 * @param b Builder
 * @param value Double value
 * @param precision Number of decimal places
 */
RGINLINE void rg_builder_append_float(RgBuilder* b, double value, int precision);

/**
 * @brief Finish building and return length
 * @param b Builder
 * @return Length of built string (excluding null terminator)
 */
RGINLINE size_t rg_builder_finish(RgBuilder* b);

/**
 * @brief Reset builder for reuse
 * @param b Builder
 */
RGINLINE void rg_builder_reset(RgBuilder* b);

// =============================================================================
// IMPLEMENTATION
// =============================================================================

#ifndef RG_SPRINTF_ASSERT
#include <assert.h>
#define RG_SPRINTF_ASSERT(x) assert(x)
#endif

// SIMD detection and includes
#ifndef RG_SPRINTF_NO_SIMD
#if defined(_MSC_VER)
#include <intrin.h>
#define RG_HAS_SSE2 1
#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
#define RG_HAS_AVX2 1
#endif
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__SSE2__)
#include <emmintrin.h>
#define RG_HAS_SSE2 1
#endif
#if defined(__SSSE3__)
#include <tmmintrin.h>
#define RG_HAS_SSSE3 1
#endif
#if defined(__AVX2__)
#include <immintrin.h>
#define RG_HAS_AVX2 1
#endif
// vqtbl1q_u8 is AArch64-only; 32-bit ARM uses the scalar encoder.
#if defined(__ARM_NEON) && defined(__aarch64__)
#include <arm_neon.h>
#define RG_HAS_NEON 1
#endif
#endif
#endif

// -----------------------------------------------------------------------------
// Lookup Tables
// -----------------------------------------------------------------------------

// Two-digit decimal pairs: "00", "01", ..., "99"
// Wrapped in struct for 2-byte alignment (enables uint16_t reads)
static struct
{
	short temp; // Force 2-byte alignment
	char pair[201];
} rg_digit_pairs_aligned =
    {
        0,
        "00010203040506070809"
        "10111213141516171819"
        "20212223242526272829"
        "30313233343536373839"
        "40414243444546474849"
        "50515253545556575859"
        "60616263646566676869"
        "70717273747576777879"
        "80818283848586878889"
        "90919293949596979899"};
#define rg_digit_pairs rg_digit_pairs_aligned.pair

// Hex lookup tables (no null terminator - used as lookup, not strings)
static const char rg_hex_upper[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
static const char rg_hex_lower[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

// Hex decode table (-1 = invalid)
static const int8_t rg_hex_decode[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,           // 0-9
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // A-F
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, // a-f
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// -----------------------------------------------------------------------------
// Internal Helpers
// -----------------------------------------------------------------------------

// Count digits in a 32-bit unsigned integer
RGINLINE int rg_count_digits_u32(uint32_t n)
{
	if (n < 10) return 1;
	if (n < 100) return 2;
	if (n < 1000) return 3;
	if (n < 10000) return 4;
	if (n < 100000) return 5;
	if (n < 1000000) return 6;
	if (n < 10000000) return 7;
	if (n < 100000000) return 8;
	if (n < 1000000000) return 9;
	return 10;
}

// Count digits in a 64-bit unsigned integer
RGINLINE int rg_count_digits_u64(uint64_t n)
{
	if (n < 10ULL) return 1;
	if (n < 100ULL) return 2;
	if (n < 1000ULL) return 3;
	if (n < 10000ULL) return 4;
	if (n < 100000ULL) return 5;
	if (n < 1000000ULL) return 6;
	if (n < 10000000ULL) return 7;
	if (n < 100000000ULL) return 8;
	if (n < 1000000000ULL) return 9;
	if (n < 10000000000ULL) return 10;
	if (n < 100000000000ULL) return 11;
	if (n < 1000000000000ULL) return 12;
	if (n < 10000000000000ULL) return 13;
	if (n < 100000000000000ULL) return 14;
	if (n < 1000000000000000ULL) return 15;
	if (n < 10000000000000000ULL) return 16;
	if (n < 100000000000000000ULL) return 17;
	if (n < 1000000000000000000ULL) return 18;
	if (n < 10000000000000000000ULL) return 19;
	return 20;
}

// -----------------------------------------------------------------------------
// Integer Conversion
// -----------------------------------------------------------------------------

// Fast unsigned 32-bit to string - writes backwards using 2-digit pairs
// Uses 16-bit writes for speed (single memory op instead of two)
RGINLINE char* rg_utoa(uint32_t value, char* buf)
{
#ifdef RG_SPRINTF_SECURE
	if (!buf) return NULL;
#endif
	char* p = buf;

	// Handle 0 specially
	if (value == 0)
	{
		*p++ = '0';
		*p = '\0';
		return p;
	}

	// Count digits and position pointer at end
	int digits = rg_count_digits_u32(value);
	p += digits;
	*p = '\0';
	char* end = p;

	// Write 2 digits at a time
	while (value >= 100)
	{
		uint32_t idx = (value % 100) * 2;
		value /= 100;
		p -= 2;
		memcpy(p, &rg_digit_pairs[idx], 2);
	}

	// Handle remaining 1-2 digits
	if (value >= 10)
	{
		p -= 2;
		memcpy(p, &rg_digit_pairs[value * 2], 2);
	}
	else
	{
		*--p = '0' + (char)value;
	}

	return end;
}

// Fast signed 32-bit to string
RGINLINE char* rg_itoa(int32_t value, char* buf)
{
#ifdef RG_SPRINTF_SECURE
	if (!buf) return NULL;
#endif
	if (value < 0)
	{
		*buf++ = '-';
		// Handle INT32_MIN specially to avoid overflow
		if (value == INT32_MIN)
		{
			return rg_utoa((uint32_t)INT32_MAX + 1, buf);
		}
		return rg_utoa((uint32_t)(-value), buf);
	}
	return rg_utoa((uint32_t)value, buf);
}

// Helper: write exactly 8 digits (for 10^8 chunks)
RGINLINE void rg_write_8_digits(char* p, uint32_t n)
{
	// Write 4 pairs of digits = 8 digits total
	uint32_t q, idx;

	q = n / 100;
	idx = (n - q * 100) * 2;
	memcpy(p + 6, &rg_digit_pairs[idx], 2);
	n = q;

	q = n / 100;
	idx = (n - q * 100) * 2;
	memcpy(p + 4, &rg_digit_pairs[idx], 2);
	n = q;

	q = n / 100;
	idx = (n - q * 100) * 2;
	memcpy(p + 2, &rg_digit_pairs[idx], 2);
	n = q;

	memcpy(p, &rg_digit_pairs[n * 2], 2);
}

// Fast unsigned 64-bit to string - uses 10^8 chunking to minimize 64-bit divisions
RGINLINE char* rg_u64toa(uint64_t value, char* buf)
{
#ifdef RG_SPRINTF_SECURE
	if (!buf) return NULL;
#endif
	char* p = buf;

	if (value == 0)
	{
		*p++ = '0';
		*p = '\0';
		return p;
	}

	// Use 32-bit path for values that fit
	if (value <= UINT32_MAX)
	{
		return rg_utoa((uint32_t)value, buf);
	}

	int digits = rg_count_digits_u64(value);
	p += digits;
	*p = '\0';
	char* end = p;

	// Process in 10^8 chunks to use fast 32-bit division
	while (value >= 100000000ULL)
	{
		uint32_t chunk = (uint32_t)(value % 100000000ULL);
		value /= 100000000ULL;
		p -= 8;
		rg_write_8_digits(p, chunk);
	}

	// Handle remaining value (now fits in 32 bits)
	uint32_t n32 = (uint32_t)value;

	// Write remaining digits
	while (n32 >= 100)
	{
		uint32_t idx = (n32 % 100) * 2;
		n32 /= 100;
		p -= 2;
		memcpy(p, &rg_digit_pairs[idx], 2);
	}

	if (n32 >= 10)
	{
		p -= 2;
		memcpy(p, &rg_digit_pairs[n32 * 2], 2);
	}
	else
	{
		*--p = '0' + (char)n32;
	}

	return end;
}

// Fast signed 64-bit to string
RGINLINE char* rg_i64toa(int64_t value, char* buf)
{
#ifdef RG_SPRINTF_SECURE
	if (!buf) return NULL;
#endif
	if (value < 0)
	{
		*buf++ = '-';
		if (value == INT64_MIN)
		{
			return rg_u64toa((uint64_t)INT64_MAX + 1, buf);
		}
		return rg_u64toa((uint64_t)(-value), buf);
	}
	return rg_u64toa((uint64_t)value, buf);
}

// -----------------------------------------------------------------------------
// Hex Conversion
// -----------------------------------------------------------------------------

// Scalar hex encoding
RGINLINE char* rg_to_hex_scalar(const uint8_t* data, size_t len, char* out, const char* hex_table)
{
	for (size_t i = 0; i < len; i++)
	{
		uint8_t byte = data[i];
		*out++ = hex_table[byte >> 4];
		*out++ = hex_table[byte & 0x0F];
	}
	*out = '\0';
	return out;
}

#if defined(RG_HAS_SSSE3) || defined(RG_HAS_NEON)
// SIMD hex encoding: 16 input bytes -> 32 hex characters per iteration using
// a byte-table lookup (pshufb on SSSE3, tbl on NEON).
RGINLINE char* rg_to_hex_simd(const uint8_t* data, size_t len, char* out, int uppercase)
{
	char* p = out;
	size_t i = 0;
#if defined(RG_HAS_NEON)
	const uint8x16_t hex_lut = vld1q_u8((const uint8_t*)(uppercase ? rg_hex_upper : rg_hex_lower));
	const uint8x16_t mask_lo = vdupq_n_u8(0x0F);

	for (; i + 16 <= len; i += 16)
	{
		uint8x16_t input = vld1q_u8(data + i);
		uint8x16x2_t pair;
		pair.val[0] = vqtbl1q_u8(hex_lut, vshrq_n_u8(input, 4));
		pair.val[1] = vqtbl1q_u8(hex_lut, vandq_u8(input, mask_lo));
		// Interleaved store writes the high nibble's character first.
		vst2q_u8((uint8_t*)p, pair);
		p += 32;
	}
#else
	__m128i hex_lut = uppercase
	                      ? _mm_setr_epi8('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F')
	                      : _mm_setr_epi8('0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f');
	__m128i mask_lo = _mm_set1_epi8(0x0F);

	// Process 16 bytes at a time -> 32 hex chars
	for (; i + 16 <= len; i += 16)
	{
		__m128i input = _mm_loadu_si128((__m128i*)(data + i));

		// Split into high and low nibbles
		__m128i lo = _mm_and_si128(input, mask_lo);
		__m128i hi = _mm_and_si128(_mm_srli_epi16(input, 4), mask_lo);

		// Lookup hex chars
		__m128i hex_lo = _mm_shuffle_epi8(hex_lut, lo);
		__m128i hex_hi = _mm_shuffle_epi8(hex_lut, hi);

		// Interleave high and low
		__m128i result_lo = _mm_unpacklo_epi8(hex_hi, hex_lo);
		__m128i result_hi = _mm_unpackhi_epi8(hex_hi, hex_lo);

		// Store results
		_mm_storeu_si128((__m128i*)p, result_lo);
		_mm_storeu_si128((__m128i*)(p + 16), result_hi);
		p += 32;
	}
#endif

	// Handle remaining bytes with scalar
	const char* hex_table = uppercase ? rg_hex_upper : rg_hex_lower;
	for (; i < len; i++)
	{
		uint8_t byte = data[i];
		*p++ = hex_table[byte >> 4];
		*p++ = hex_table[byte & 0x0F];
	}

	*p = '\0';
	return p;
}
#endif

RGINLINE char* rg_to_hex(const void* data, size_t len, char* out, int uppercase)
{
	const uint8_t* bytes = (const uint8_t*)data;

#if defined(RG_HAS_SSSE3) || defined(RG_HAS_NEON)
	if (len >= 16)
	{
		return rg_to_hex_simd(bytes, len, out, uppercase);
	}
#endif

	const char* hex_table = uppercase ? rg_hex_upper : rg_hex_lower;
	return rg_to_hex_scalar(bytes, len, out, hex_table);
}

RGINLINE int rg_from_hex(const char* hex, size_t hex_len, void* out)
{
	if (hex_len % 2 != 0) return -1;

	uint8_t* bytes = (uint8_t*)out;
	size_t byte_len = hex_len / 2;

	for (size_t i = 0; i < byte_len; i++)
	{
		int8_t hi = rg_hex_decode[(uint8_t)hex[i * 2]];
		int8_t lo = rg_hex_decode[(uint8_t)hex[i * 2 + 1]];
		if (hi < 0 || lo < 0) return -1;
		bytes[i] = (uint8_t)((hi << 4) | lo);
	}

	return (int)byte_len;
}

// -----------------------------------------------------------------------------
// Float Conversion (Fast Approximate)
// -----------------------------------------------------------------------------

typedef union
{
	double d;
	uint64_t u;
} rg_double_bits;

// -----------------------------------------------------------------------------
// Exact binary-to-decimal conversion
// -----------------------------------------------------------------------------
//
// Every digit is derived from the exact value of the double with integer
// arithmetic, so the output matches a correctly rounded libc (glibc, musl,
// macOS, FreeBSD) bit for bit. Values a game prints use 128-bit integer
// arithmetic; a small bignum handles exponents beyond that range.
//
// Rounding: round half to even on the exact binary value, which is what IEEE
// 754 and the libcs above do. RG_SPRINTF_ROUND_TIES_AWAY=1 rounds exact ties
// away from zero instead, matching Microsoft's CRT.

#ifndef RG_SPRINTF_ROUND_TIES_AWAY
#define RG_SPRINTF_ROUND_TIES_AWAY 0
#endif

// Out-of-line, rarely executed helpers (bignum paths) so the common conversions
// stay small when inlined.
#ifndef RG_SPRINTF_COLD
#if defined(_MSC_VER)
#define RG_SPRINTF_COLD static __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define RG_SPRINTF_COLD static __attribute__((noinline, cold))
#else
#define RG_SPRINTF_COLD static
#endif
#endif

#ifndef RG_SPRINTF_UNLIKELY
#if defined(__GNUC__) || defined(__clang__)
#define RG_SPRINTF_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define RG_SPRINTF_UNLIKELY(x) (x)
#endif
#endif

// Fractional digits above this are clamped. 350 covers every digit a double
// can produce before the expansion ends (2^-1074 has 1074 digits, but libc
// callers rarely ask for more than a few hundred).
#define RG_SPRINTF_FLOAT_MAX_PRECISION 350
// Longest rg_dtoa output: sign, 309 integer digits, point, fraction, terminator.
#define RG_SPRINTF_FLOAT_BUFFER_SIZE (1 + 309 + 1 + RG_SPRINTF_FLOAT_MAX_PRECISION + 1)

// -----------------------------------------------------------------------------
// 128-bit helpers
// -----------------------------------------------------------------------------

typedef struct
{
	uint64_t lo;
	uint64_t hi;
} rg_u128;

RGINLINE rg_u128 rg_u128_from_u64(uint64_t v)
{
	rg_u128 r;
	r.lo = v;
	r.hi = 0;
	return r;
}

RGINLINE rg_u128 rg_u128_mul(uint64_t a, uint64_t b)
{
	rg_u128 r;
#if defined(__SIZEOF_INT128__)
	unsigned __int128 p = (unsigned __int128)a * b;
	r.lo = (uint64_t)p;
	r.hi = (uint64_t)(p >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
	r.lo = _umul128(a, b, &r.hi);
#else
	uint64_t a_lo = (uint32_t)a, a_hi = a >> 32, b_lo = (uint32_t)b, b_hi = b >> 32;
	uint64_t p0 = a_lo * b_lo, p1 = a_lo * b_hi, p2 = a_hi * b_lo, p3 = a_hi * b_hi;
	uint64_t mid = (p0 >> 32) + (uint32_t)p1 + (uint32_t)p2;
	r.lo = (mid << 32) | (uint32_t)p0;
	r.hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
#endif
	return r;
}

// a >> shift for 0 < shift < 128.
RGINLINE rg_u128 rg_u128_shr(rg_u128 a, int shift)
{
	rg_u128 r;
	if (shift >= 64)
	{
		r.lo = a.hi >> (shift - 64);
		r.hi = 0;
	}
	else
	{
		r.lo = (a.lo >> shift) | (a.hi << (64 - shift));
		r.hi = a.hi >> shift;
	}
	return r;
}

// a << shift for 0 <= shift < 128 (bits shifted out are lost).
RGINLINE rg_u128 rg_u128_shl(rg_u128 a, int shift)
{
	rg_u128 r;
	if (shift == 0) return a;
	if (shift >= 64)
	{
		r.hi = a.lo << (shift - 64);
		r.lo = 0;
	}
	else
	{
		r.hi = (a.hi << shift) | (a.lo >> (64 - shift));
		r.lo = a.lo << shift;
	}
	return r;
}

RGINLINE int rg_u128_cmp(rg_u128 a, rg_u128 b)
{
	if (a.hi != b.hi) return a.hi < b.hi ? -1 : 1;
	if (a.lo != b.lo) return a.lo < b.lo ? -1 : 1;
	return 0;
}

// Where the discarded remainder sits relative to half a unit in the last place.
// RG_REM_ZERO means the conversion was exact.
#define RG_REM_ZERO 0
#define RG_REM_BELOW 1
#define RG_REM_HALF 2
#define RG_REM_ABOVE 3

// Branch-free: whether the remainder sits above, at, or below the half point
// is a coin flip on real data, so it is computed with flag arithmetic.
RGINLINE int rg_rem_state_u64(uint64_t rem, uint64_t half)
{
	return (int)((rem != 0) + (rem >= half) + (rem > half));
}

// State of remainder r against divisor d (compares 2r with d; r < 2^127).
RGINLINE int rg_rem_state_u128(rg_u128 r, rg_u128 d)
{
	if (r.lo == 0 && r.hi == 0) return RG_REM_ZERO;
	int cmp = rg_u128_cmp(rg_u128_shl(r, 1), d);
	return cmp < 0 ? RG_REM_BELOW : (cmp == 0 ? RG_REM_HALF : RG_REM_ABOVE);
}

// Remainder state of a quotient q scaled down by one decimal digit: the digit
// dropped from q joins the old remainder.
RGINLINE int rg_rem_state_drop_digit(uint64_t dropped, int state)
{
	// Row: dropped digit; column: previous state.
	static const uint8_t table[10][4] = {
	    {RG_REM_ZERO, RG_REM_BELOW, RG_REM_BELOW, RG_REM_BELOW},
	    {RG_REM_BELOW, RG_REM_BELOW, RG_REM_BELOW, RG_REM_BELOW},
	    {RG_REM_BELOW, RG_REM_BELOW, RG_REM_BELOW, RG_REM_BELOW},
	    {RG_REM_BELOW, RG_REM_BELOW, RG_REM_BELOW, RG_REM_BELOW},
	    {RG_REM_BELOW, RG_REM_BELOW, RG_REM_BELOW, RG_REM_BELOW},
	    {RG_REM_HALF, RG_REM_ABOVE, RG_REM_ABOVE, RG_REM_ABOVE},
	    {RG_REM_ABOVE, RG_REM_ABOVE, RG_REM_ABOVE, RG_REM_ABOVE},
	    {RG_REM_ABOVE, RG_REM_ABOVE, RG_REM_ABOVE, RG_REM_ABOVE},
	    {RG_REM_ABOVE, RG_REM_ABOVE, RG_REM_ABOVE, RG_REM_ABOVE},
	    {RG_REM_ABOVE, RG_REM_ABOVE, RG_REM_ABOVE, RG_REM_ABOVE}};
	return table[dropped][state];
}

// Low 'bits' bits of a, 0 < bits < 128.
RGINLINE rg_u128 rg_u128_low_bits(rg_u128 a, int bits)
{
	rg_u128 r;
	if (bits >= 64)
	{
		r.lo = a.lo;
		r.hi = bits == 64 ? 0 : a.hi & ((1ull << (bits - 64)) - 1u);
	}
	else
	{
		r.lo = a.lo & ((1ull << bits) - 1u);
		r.hi = 0;
	}
	return r;
}

// Unsigned 128 by 64 division with remainder; the quotient must fit 64 bits.
RGINLINE uint64_t rg_u128_div_u64(rg_u128 n, uint64_t d, uint64_t* rem)
{
#if defined(__SIZEOF_INT128__)
	unsigned __int128 v = ((unsigned __int128)n.hi << 64) | n.lo;
	*rem = (uint64_t)(v % d);
	return (uint64_t)(v / d);
#elif defined(_MSC_VER) && defined(_M_X64) && _MSC_VER >= 1920
	return _udiv128(n.hi, n.lo, d, rem);
#else
	// Shift-subtract; only reached on compilers without a 128-bit divide.
	uint64_t q = 0, r = 0;
	for (int i = 127; i >= 0; i--)
	{
		uint64_t bit = i >= 64 ? (n.hi >> (i - 64)) & 1u : (n.lo >> i) & 1u;
		int carry = (int)(r >> 63);
		r = (r << 1) | bit;
		if (carry || r >= d)
		{
			r -= d;
			q |= (i < 64) ? (1ull << i) : 0;
		}
	}
	*rem = r;
	return q;
#endif
}

RGINLINE int rg_bit_length_u64(uint64_t v)
{
#if defined(__GNUC__) || defined(__clang__)
	return v ? 64 - __builtin_clzll(v) : 0;
#elif defined(_MSC_VER) && defined(_M_X64)
	unsigned long idx;
	return _BitScanReverse64(&idx, v) ? (int)idx + 1 : 0;
#else
	int n = 0;
	while (v)
	{
		v >>= 1;
		n++;
	}
	return n;
#endif
}

static const uint64_t rg_pow5_u64[28] = {
    1ull, 5ull, 25ull, 125ull, 625ull, 3125ull, 15625ull, 78125ull, 390625ull, 1953125ull,
    9765625ull, 48828125ull, 244140625ull, 1220703125ull, 6103515625ull, 30517578125ull,
    152587890625ull, 762939453125ull, 3814697265625ull, 19073486328125ull, 95367431640625ull,
    476837158203125ull, 2384185791015625ull, 11920928955078125ull, 59604644775390625ull,
    298023223876953125ull, 1490116119384765625ull, 7450580596923828125ull};

static const uint64_t rg_float_pow10_u64[20] = {
    1ull, 10ull, 100ull, 1000ull, 10000ull, 100000ull, 1000000ull, 10000000ull, 100000000ull,
    1000000000ull, 10000000000ull, 100000000000ull, 1000000000000ull, 10000000000000ull,
    100000000000000ull, 1000000000000000ull, 10000000000000000ull, 100000000000000000ull,
    1000000000000000000ull, 10000000000000000000ull};

// Decimal digit count of v > 0 from its bit length and one table compare.
RGINLINE int rg_float_count_digits(uint64_t v)
{
	int t = ((rg_bit_length_u64(v) * 1233) >> 12) + 1; // 1 + floor(log2(v) * log10(2)), at most one high
	return t - (v < rg_float_pow10_u64[t - 1]);
}

// Whether the truncated result rounds up: always above half, never below, and
// on an exact tie when the last kept digit is odd (ties-to-even) or always
// (ties-away).
RGINLINE int rg_float_round_up(int state, uint64_t low_digit)
{
#if RG_SPRINTF_ROUND_TIES_AWAY
	(void)low_digit;
	return state >> 1; // half or above
#else
	// Above: 1. Half: the parity of the last kept digit. Otherwise 0.
	return (state >> 1) & ((unsigned int)state | (unsigned int)low_digit) & 1;
#endif
}

// -----------------------------------------------------------------------------
// Small bignum: little-endian 32-bit limbs, enough for 2^1024 * 5^700.
// -----------------------------------------------------------------------------

#define RG_BIG_LIMBS 60

typedef struct
{
	uint32_t limb[RG_BIG_LIMBS];
	int count; // used limbs; 0 means zero
} rg_bigint;

RGINLINE void rg_big_from_u64(rg_bigint* b, uint64_t v)
{
	b->limb[0] = (uint32_t)v;
	b->limb[1] = (uint32_t)(v >> 32);
	b->count = b->limb[1] ? 2 : (b->limb[0] ? 1 : 0);
}

RG_SPRINTF_COLD void rg_big_mul_u32(rg_bigint* b, uint32_t m)
{
	uint64_t carry = 0;
	for (int i = 0; i < b->count; i++)
	{
		uint64_t p = (uint64_t)b->limb[i] * m + carry;
		b->limb[i] = (uint32_t)p;
		carry = p >> 32;
	}
	if (carry && b->count < RG_BIG_LIMBS) b->limb[b->count++] = (uint32_t)carry;
}

// b *= 5^n
RG_SPRINTF_COLD void rg_big_mul_pow5(rg_bigint* b, int n)
{
	while (n >= 13)
	{
		rg_big_mul_u32(b, 1220703125u); // 5^13
		n -= 13;
	}
	if (n > 0) rg_big_mul_u32(b, (uint32_t)rg_pow5_u64[n]);
}

RG_SPRINTF_COLD void rg_big_shl(rg_bigint* b, int bits)
{
	int limbs = bits >> 5;
	int rem = bits & 31;
	if (b->count == 0) return;
	if (rem)
	{
		uint32_t carry = 0;
		for (int i = 0; i < b->count; i++)
		{
			uint32_t v = b->limb[i];
			b->limb[i] = (v << rem) | carry;
			carry = v >> (32 - rem);
		}
		if (carry && b->count < RG_BIG_LIMBS) b->limb[b->count++] = carry;
	}
	if (limbs)
	{
		int new_count = b->count + limbs;
		if (new_count > RG_BIG_LIMBS) new_count = RG_BIG_LIMBS;
		for (int i = new_count - 1; i >= limbs; i--) b->limb[i] = b->limb[i - limbs];
		for (int i = 0; i < limbs && i < new_count; i++) b->limb[i] = 0;
		b->count = new_count;
	}
}

// b >>= bits. Returns the RG_REM_* state of the discarded part.
RG_SPRINTF_COLD int rg_big_shr(rg_bigint* b, int bits)
{
	int limbs = bits >> 5;
	int rem = bits & 31;
	int cmp;
	if (bits <= 0) return RG_REM_ZERO;
	// Half bit and whether anything below it is set.
	int half_index = bits - 1;
	int half_set = (half_index >> 5) < b->count && ((b->limb[half_index >> 5] >> (half_index & 31)) & 1u);
	int below_set = 0;
	for (int i = 0; i < (half_index >> 5) && i < b->count && !below_set; i++) below_set |= b->limb[i] != 0;
	if (!below_set && (half_index >> 5) < b->count && (half_index & 31))
		below_set |= (b->limb[half_index >> 5] & ((1u << (half_index & 31)) - 1u)) != 0;
	cmp = half_set ? (below_set ? RG_REM_ABOVE : RG_REM_HALF) : (below_set ? RG_REM_BELOW : RG_REM_ZERO);
	if (limbs >= b->count)
	{
		b->count = 0;
		return cmp;
	}
	for (int i = 0; i + limbs < b->count; i++) b->limb[i] = b->limb[i + limbs];
	b->count -= limbs;
	if (rem)
	{
		for (int i = 0; i < b->count; i++)
		{
			uint32_t next = (i + 1 < b->count) ? b->limb[i + 1] : 0;
			b->limb[i] = (b->limb[i] >> rem) | (next << (32 - rem));
		}
	}
	while (b->count > 0 && b->limb[b->count - 1] == 0) b->count--;
	return cmp;
}

RG_SPRINTF_COLD uint32_t rg_big_divmod_u32(rg_bigint* b, uint32_t d)
{
	uint64_t rem = 0;
	for (int i = b->count - 1; i >= 0; i--)
	{
		uint64_t cur = (rem << 32) | b->limb[i];
		b->limb[i] = (uint32_t)(cur / d);
		rem = cur % d;
	}
	while (b->count > 0 && b->limb[b->count - 1] == 0) b->count--;
	return (uint32_t)rem;
}

// Writes the decimal digits of b (destroying it) into out and returns the
// digit count. Writes "0" for zero.
RG_SPRINTF_COLD int rg_big_to_digits(rg_bigint* b, char* out)
{
	uint32_t chunks[128];
	int n = 0;
	if (b->count == 0)
	{
		out[0] = '0';
		return 1;
	}
	while (b->count > 0 && n < 128) chunks[n++] = rg_big_divmod_u32(b, 1000000000u);
	char* p = out;
	// Leading chunk without padding, the rest zero-padded to 9 digits.
	char tmp[12];
	char* e = rg_u64toa(chunks[n - 1], tmp);
	memcpy(p, tmp, (size_t)(e - tmp));
	p += e - tmp;
	for (int i = n - 2; i >= 0; i--)
	{
		uint32_t c = chunks[i];
		for (int k = 8; k >= 0; k--)
		{
			p[k] = (char)('0' + c % 10u);
			c /= 10u;
		}
		p += 9;
	}
	return (int)(p - out);
}

// -----------------------------------------------------------------------------
// Digit emission
// -----------------------------------------------------------------------------

// Writes exactly 'digits' decimal digits of v (v < 10^digits), zero padded.
// Runs of up to eight digits use independent divisions so the multiplies can
// overlap; longer runs peel pairs from the end.
RGINLINE char* rg_write_digits_padded(char* p, uint64_t v, int digits)
{
	// Up to eight digits fit 32 bits, which keeps the constant divisions on the
	// cheaper 32-bit multiplier.
	uint32_t w = (uint32_t)v;
	switch (digits)
	{
		case 0: return p;
		case 1:
			p[0] = (char)('0' + w);
			return p + 1;
		case 2:
			memcpy(p, &rg_digit_pairs[w * 2], 2);
			return p + 2;
		case 3:
			p[0] = (char)('0' + w / 100u);
			memcpy(p + 1, &rg_digit_pairs[(w % 100u) * 2], 2);
			return p + 3;
		case 4:
			memcpy(p, &rg_digit_pairs[(w / 100u) * 2], 2);
			memcpy(p + 2, &rg_digit_pairs[(w % 100u) * 2], 2);
			return p + 4;
		case 5:
			p[0] = (char)('0' + w / 10000u);
			memcpy(p + 1, &rg_digit_pairs[((w / 100u) % 100u) * 2], 2);
			memcpy(p + 3, &rg_digit_pairs[(w % 100u) * 2], 2);
			return p + 5;
		case 6:
			memcpy(p, &rg_digit_pairs[(w / 10000u) * 2], 2);
			memcpy(p + 2, &rg_digit_pairs[((w / 100u) % 100u) * 2], 2);
			memcpy(p + 4, &rg_digit_pairs[(w % 100u) * 2], 2);
			return p + 6;
		case 7:
		{
			uint32_t low = w % 1000000u;
			p[0] = (char)('0' + w / 1000000u);
			memcpy(p + 1, &rg_digit_pairs[(low / 10000u) * 2], 2);
			memcpy(p + 3, &rg_digit_pairs[((low / 100u) % 100u) * 2], 2);
			memcpy(p + 5, &rg_digit_pairs[(low % 100u) * 2], 2);
			return p + 7;
		}
		case 8:
		{
			uint32_t low = w % 1000000u;
			memcpy(p, &rg_digit_pairs[(w / 1000000u) * 2], 2);
			memcpy(p + 2, &rg_digit_pairs[(low / 10000u) * 2], 2);
			memcpy(p + 4, &rg_digit_pairs[((low / 100u) % 100u) * 2], 2);
			memcpy(p + 6, &rg_digit_pairs[(low % 100u) * 2], 2);
			return p + 8;
		}
		default:
		{
			char* end = p + digits;
			char* q = end;
			while (q - p >= 2)
			{
				uint64_t pair = v % 100u;
				v /= 100u;
				q -= 2;
				memcpy(q, &rg_digit_pairs[pair * 2], 2);
			}
			if (q > p) *p = (char)('0' + v);
			return end;
		}
	}
}

// Adds one to the decimal digit string [digits, digits + n). Returns 1 when
// the carry runs off the front (the string was all nines and is now "0...0"
// and the caller must prepend a '1').
RGINLINE int rg_digits_increment(char* digits, int n)
{
	for (int i = n - 1; i >= 0; i--)
	{
		if (digits[i] != '9')
		{
			digits[i]++;
			return 0;
		}
		digits[i] = '0';
	}
	return 1;
}

// -----------------------------------------------------------------------------
// Decomposition
// -----------------------------------------------------------------------------

// value = m * 2^e2 for a positive finite double (zero gives m = 0).
RGINLINE void rg_double_decompose(double value, uint64_t* m, int* e2)
{
	rg_double_bits bits;
	bits.d = value;
	uint64_t mant = bits.u & 0xFFFFFFFFFFFFFull;
	int exp = (int)((bits.u >> 52) & 0x7FF);
	if (exp == 0)
	{
		*m = mant;
		*e2 = -1074;
	}
	else
	{
		*m = mant | (1ull << 52);
		*e2 = exp - 1075;
	}
}

// -----------------------------------------------------------------------------
// Fixed notation: integer digits, point, exactly 'precision' fraction digits.
// value is positive and finite; precision is 0..RG_SPRINTF_FLOAT_MAX_PRECISION.
// -----------------------------------------------------------------------------

RG_SPRINTF_COLD char* rg_dtoa_fixed_big(uint64_t m, int e2, char* p, int precision)
{
	rg_bigint b;
	if (e2 >= 0)
	{
		rg_big_from_u64(&b, m);
		rg_big_shl(&b, e2);
		p += rg_big_to_digits(&b, p);
		if (precision > 0)
		{
			*p++ = '.';
			memset(p, '0', (size_t)precision);
			p += precision;
		}
		return p;
	}
	int k = -e2;
	uint64_t int_part = k < 64 ? (m >> k) : 0;
	uint64_t frac_bits = k < 64 ? (m & ((1ull << k) - 1u)) : m;
	// frac * 10^precision = frac_bits * 5^precision * 2^(precision - k)
	rg_big_from_u64(&b, frac_bits);
	rg_big_mul_pow5(&b, precision);
	int round_up = 0;
	if (precision >= k)
	{
		rg_big_shl(&b, precision - k);
	}
	else
	{
		int state = rg_big_shr(&b, k - precision);
		// Ties look at the last kept digit: the integer part when there is no fraction.
		round_up = rg_float_round_up(state, precision ? (b.count ? b.limb[0] : 0u) : int_part);
	}
	char frac[RG_SPRINTF_FLOAT_MAX_PRECISION + 2];
	int n = rg_big_to_digits(&b, frac); // n <= precision: the fraction is below 10^precision
	if (round_up && rg_digits_increment(frac, n))
	{
		// All nines rounded up: the digits are now zeros and the value is 10^n.
		if (n >= precision)
		{
			int_part++; // 10^precision carries into the integer part
			n = 1;
		}
		else
		{
			frac[0] = '1';
			frac[n++] = '0';
		}
	}
	p = rg_u64toa(int_part, p);
	if (precision > 0)
	{
		*p++ = '.';
		memset(p, '0', (size_t)(precision - n));
		p += precision - n;
		memcpy(p, frac, (size_t)n);
		p += n;
	}
	return p;
}

// Fixed notation for the general case: any exponent, any precision.
RG_SPRINTF_COLD char* rg_dtoa_fixed_general(uint64_t m, int e2, char* p, int precision)
{
	if (e2 >= 0)
	{
		if (e2 > 11) return rg_dtoa_fixed_big(m, e2, p, precision);
		p = rg_u64toa(m << e2, p);
		if (precision > 0)
		{
			*p++ = '.';
			memset(p, '0', (size_t)precision);
			p += precision;
		}
		return p;
	}
	if (precision > 17) return rg_dtoa_fixed_big(m, e2, p, precision);

	int k = -e2;
	uint64_t int_part = k < 64 ? (m >> k) : 0;
	uint64_t frac_bits = k < 64 ? (m & ((1ull << k) - 1u)) : m;
	uint64_t q;
	// frac * 10^precision = (frac_bits * 5^precision) / 2^(k - precision)
	rg_u128 n = rg_u128_mul(frac_bits, rg_pow5_u64[precision]);
	int shift = k - precision;
	if (shift <= 0)
	{
		q = n.lo << -shift; // exact: the result is below 10^precision
	}
	else
	{
		int state = RG_REM_ZERO;
		if (shift < 64)
		{
			// The quotient is below 10^precision, so no bits above n.hi << (64 - shift) remain.
			q = (n.lo >> shift) | (n.hi << (64 - shift));
			state = rg_rem_state_u64(n.lo & ((1ull << shift) - 1u), 1ull << (shift - 1));
		}
		else if (shift < 128)
		{
			q = n.hi >> (shift - 64);
			uint64_t rem_hi = n.hi & ((shift == 64) ? 0u : ((1ull << (shift - 64)) - 1u));
			uint64_t half_hi = (shift == 64) ? 0u : (1ull << (shift - 65));
			if (shift == 64)
				state = rg_rem_state_u64(n.lo, 1ull << 63);
			else if (rem_hi != half_hi)
				state = (rem_hi == 0 && n.lo == 0) ? RG_REM_ZERO : (rem_hi < half_hi ? RG_REM_BELOW : RG_REM_ABOVE);
			else
				state = n.lo == 0 ? RG_REM_HALF : RG_REM_ABOVE;
		}
		else
		{
			q = 0; // below half of one unit in the last place
			state = RG_REM_BELOW;
		}
		q += (uint64_t)rg_float_round_up(state, precision ? q : int_part);
		if (q == rg_float_pow10_u64[precision])
		{
			q = 0;
			int_part++;
		}
	}
	p = rg_u64toa(int_part, p);
	if (precision > 0)
	{
		*p++ = '.';
		p = rg_write_digits_padded(p, q, precision);
	}
	return p;
}

// Fixed notation: integer digits, point, exactly 'precision' fraction digits.
// value is positive and finite; precision is 0..RG_SPRINTF_FLOAT_MAX_PRECISION.
// The straight-line path covers magnitudes in [2^-11, 2^53) with at most 17
// places, where the scaled fraction and its remainder fit one 64-bit word each.
RGINLINE char* rg_dtoa_fixed_exact(double value, char* p, int precision)
{
	uint64_t m;
	int e2;
	rg_double_decompose(value, &m, &e2);
	if (m == 0)
	{
		// Zero is common in logs; its digits need no arithmetic.
		*p++ = '0';
		if (precision > 0)
		{
			*p++ = '.';
			memset(p, '0', (size_t)precision);
			p += precision;
		}
		return p;
	}
	int shift = -e2 - precision; // divide frac_bits * 5^precision by 2^shift
	if (e2 >= 0 || e2 < -63 || precision > 17 || shift <= 0 || shift >= 64)
	{
		return rg_dtoa_fixed_general(m, e2, p, precision);
	}
	int k = -e2;
	uint64_t int_part = m >> k;
	uint64_t frac_bits = m & ((1ull << k) - 1u);
	rg_u128 n = rg_u128_mul(frac_bits, rg_pow5_u64[precision]);
	uint64_t q = (n.lo >> shift) | (n.hi << (64 - shift));
	uint64_t rem = n.lo & ((1ull << shift) - 1u);
	uint64_t half = 1ull << (shift - 1);
	// rem >= half without a data-dependent branch; half < 2^63 so the
	// subtraction only wraps when the remainder is at least half.
	uint64_t up = (half - rem - 1u) >> 63;
#if !RG_SPRINTF_ROUND_TIES_AWAY
	if (RG_SPRINTF_UNLIKELY(rem == half)) up = (precision ? q : int_part) & 1u;
#endif
	q += up;
	if (q == rg_float_pow10_u64[precision])
	{
		q = 0;
		int_part++;
	}
	p = rg_u64toa(int_part, p);
	if (precision > 0)
	{
		*p++ = '.';
		p = rg_write_digits_padded(p, q, precision);
	}
	return p;
}

// -----------------------------------------------------------------------------
// Scientific notation digits: precision + 1 significant digits as "d.ddd",
// returning the decimal exponent. value is positive and finite.
// -----------------------------------------------------------------------------

// Computes D = floor(m * 2^e2 * 10^s) and the RG_REM_* state of the remainder.
// Returns 1 on success, 0 when the operands need the bignum path, and -1 when
// the quotient does not fit 64 bits (the exponent estimate was too low).
RGINLINE int rg_sci_scaled_fast(uint64_t m, int e2, int s, uint64_t* d_out, int* state_out)
{
	if (s >= 0)
	{
		if (s > 27) return 0;
		rg_u128 a = rg_u128_mul(m, rg_pow5_u64[s]);
		int t = e2 + s;
		if (t >= 0)
		{
			if (a.hi || t > 10 || (a.lo >> (64 - t - 1)) > 1u) return a.hi ? 0 : -1;
			*d_out = a.lo << t;
			*state_out = RG_REM_ZERO;
			return 1;
		}
		int shift = -t;
		if (shift < 64)
		{
			if ((a.hi >> shift) != 0) return -1;
			*d_out = (a.lo >> shift) | (a.hi << (64 - shift));
			*state_out = rg_rem_state_u64(a.lo & ((1ull << shift) - 1u), 1ull << (shift - 1));
			return 1;
		}
		if (shift >= 128)
		{
			*d_out = 0;
			*state_out = (a.lo | a.hi) ? RG_REM_BELOW : RG_REM_ZERO;
			return 1;
		}
		rg_u128 quotient = rg_u128_shr(a, shift);
		rg_u128 rem = rg_u128_low_bits(a, shift);
		rg_u128 half;
		if (shift - 1 >= 64)
		{
			half.lo = 0;
			half.hi = 1ull << (shift - 65);
		}
		else
		{
			half.lo = 1ull << (shift - 1);
			half.hi = 0;
		}
		*d_out = quotient.lo;
		*state_out = rg_rem_state_u128(rem, rg_u128_shl(half, 1));
		return 1;
	}
	int u = -s;
	if (u > 27) return 0;
	uint64_t den5 = rg_pow5_u64[u];
	int w = u - e2; // divisor is 5^u * 2^w
	if (w <= 0)
	{
		int shift = -w;
		if (shift > 74) return 0;
		rg_u128 num = rg_u128_shl(rg_u128_from_u64(m), shift);
		if (num.hi >= den5) return -1;
		uint64_t rem;
		*d_out = rg_u128_div_u64(num, den5, &rem);
		*state_out = rg_rem_state_u128(rg_u128_from_u64(rem), rg_u128_from_u64(den5));
		return 1;
	}
	if (w >= 64)
	{
		*d_out = 0;
		*state_out = RG_REM_BELOW;
		return 1;
	}
	uint64_t hi = m >> w;
	uint64_t low_bits = m & ((1ull << w) - 1u);
	uint64_t rem_hi = hi % den5;
	*d_out = hi / den5;
	// remainder = rem_hi * 2^w + low_bits, divisor = den5 * 2^w
	rg_u128 rem = rg_u128_shl(rg_u128_from_u64(rem_hi), w);
	rem.lo |= low_bits;
	rg_u128 den = rg_u128_shl(rg_u128_from_u64(den5), w);
	*state_out = rg_rem_state_u128(rem, den);
	return 1;
}

// Bignum version producing the digit string directly: writes the digits of
// floor(value * 10^s) to out, returns the count, and sets *state_out.
RG_SPRINTF_COLD int rg_sci_scaled_big(uint64_t m, int e2, int s, char* out, int* state_out)
{
	rg_bigint b;
	rg_big_from_u64(&b, m);
	if (s >= 0)
	{
		rg_big_mul_pow5(&b, s);
		int t = e2 + s;
		if (t >= 0)
		{
			rg_big_shl(&b, t);
			*state_out = RG_REM_ZERO;
		}
		else
		{
			*state_out = rg_big_shr(&b, -t);
		}
		return rg_big_to_digits(&b, out);
	}
	// s < 0 only reaches here for values of at least 10^28, which are integers:
	// write every digit, then drop the last -s digits, remembering what they were.
	rg_big_shl(&b, e2);
	char all[400];
	int n = rg_big_to_digits(&b, all);
	int keep = n + s;
	if (keep <= 0)
	{
		out[0] = '0';
		*state_out = RG_REM_BELOW;
		return 1;
	}
	memcpy(out, all, (size_t)keep);
	// Dropped digits compared with 5000...: the first decides unless it is 5 or 0.
	char first = all[keep];
	int rest_nonzero = 0;
	for (int i = keep + 1; i < n && !rest_nonzero; i++) rest_nonzero |= all[i] != '0';
	if (first > '5') *state_out = RG_REM_ABOVE;
	else if (first == '5') *state_out = rest_nonzero ? RG_REM_ABOVE : RG_REM_HALF;
	else if (first == '0' && !rest_nonzero) *state_out = RG_REM_ZERO;
	else *state_out = RG_REM_BELOW;
	return keep;
}

// Slow path of rg_dtoa_sci_exact: retries the exponent estimate and falls back
// to the bignum conversion. E is the current estimate.
RG_SPRINTF_COLD char* rg_dtoa_sci_slow(uint64_t m, int e2, char* p, int precision, int E, int* exp10)
{
	int digits_wanted = precision + 1;
	for (int attempt = 0; attempt < 8; attempt++)
	{
		int s = precision - E;
		uint64_t d = 0;
		int state = RG_REM_ZERO;
		int ok = digits_wanted <= 17 ? rg_sci_scaled_fast(m, e2, s, &d, &state) : 0;
		if (ok < 0)
		{
			E++;
			continue;
		}
		if (ok)
		{
			if (d == 0)
			{
				E--;
				continue;
			}
			int n = rg_float_count_digits(d);
			if (n < digits_wanted)
			{
				E--;
				continue;
			}
			while (n > digits_wanted)
			{
				state = rg_rem_state_drop_digit(d % 10u, state);
				d /= 10u;
				E++;
				n--;
			}
			d += (uint64_t)rg_float_round_up(state, d);
			if (d == rg_float_pow10_u64[digits_wanted])
			{
				d = rg_float_pow10_u64[digits_wanted - 1];
				E++;
			}
			rg_write_digits_padded(p + 1, d, digits_wanted);
			p[0] = p[1];
			*exp10 = E;
			if (precision == 0) return p + 1;
			p[1] = '.';
			return p + 1 + digits_wanted;
		}
		char digits[RG_SPRINTF_FLOAT_MAX_PRECISION + 3];
		int n = rg_sci_scaled_big(m, e2, s, digits, &state);
		if (digits[0] == '0' && n == 1)
		{
			E--;
			continue;
		}
		if (n > digits_wanted)
		{
			E++;
			continue;
		}
		if (n < digits_wanted)
		{
			E--;
			continue;
		}
		if (rg_float_round_up(state, (uint64_t)(digits[n - 1] - '0')) && rg_digits_increment(digits, n))
		{
			// 99..9 rounded up to 100..0: one more digit, so shift the exponent.
			digits[0] = '1';
			E++;
		}
		*p++ = digits[0];
		if (precision > 0)
		{
			*p++ = '.';
			memcpy(p, digits + 1, (size_t)precision);
			p += precision;
		}
		*exp10 = E;
		return p;
	}
	// Unreachable: the estimate is within two of the exponent.
	*exp10 = E;
	return p;
}

// Writes precision + 1 significant digits with a point after the first
// (no point when precision is 0) and stores the decimal exponent.
RGINLINE char* rg_dtoa_sci_exact(double value, char* p, int precision, int* exp10)
{
	uint64_t m;
	int e2;
	rg_double_decompose(value, &m, &e2);
	if (m == 0)
	{
		*p++ = '0';
		if (precision > 0)
		{
			*p++ = '.';
			memset(p, '0', (size_t)precision);
			p += precision;
		}
		*exp10 = 0;
		return p;
	}
	// Estimate floor(log10(value)) from floor(log2(value)), never above the
	// truth and at most two below it; a low estimate yields extra digits that
	// are divided away exactly.
	int log2_floor = rg_bit_length_u64(m) - 1 + e2;
	int E = log2_floor >= 0 ? (log2_floor * 1233) >> 12 : -((((-log2_floor) * 1233) + 4095) >> 12) - 1;
	int digits_wanted = precision + 1;
	uint64_t d = 0;
	int state = RG_REM_ZERO;
	if (RG_SPRINTF_UNLIKELY(digits_wanted > 17) || RG_SPRINTF_UNLIKELY(rg_sci_scaled_fast(m, e2, precision - E, &d, &state) != 1) ||
	    RG_SPRINTF_UNLIKELY(d == 0))
	{
		return rg_dtoa_sci_slow(m, e2, p, precision, E, exp10);
	}
	int n = rg_float_count_digits(d);
	if (RG_SPRINTF_UNLIKELY(n < digits_wanted)) return rg_dtoa_sci_slow(m, e2, p, precision, E, exp10);
	while (n > digits_wanted)
	{
		state = rg_rem_state_drop_digit(d % 10u, state);
		d /= 10u;
		E++;
		n--;
	}
	d += (uint64_t)rg_float_round_up(state, d);
	if (d == rg_float_pow10_u64[digits_wanted])
	{
		d = rg_float_pow10_u64[digits_wanted - 1];
		E++;
	}
	// Digits land one past the start so the point can be inserted without a
	// second pass.
	rg_write_digits_padded(p + 1, d, digits_wanted);
	p[0] = p[1];
	*exp10 = E;
	if (precision == 0) return p + 1;
	p[1] = '.';
	return p + 1 + digits_wanted;
}

RGINLINE char* rg_dtoa(double value, char* buf, int precision)
{
	char* p = buf;
	rg_double_bits bits;
	bits.d = value;

	if (bits.u >> 63)
	{
		*p++ = '-';
		bits.u &= 0x7FFFFFFFFFFFFFFFull;
		value = bits.d;
	}

	if (((bits.u >> 52) & 0x7FF) == 0x7FF)
	{
		memcpy(p, (bits.u & 0xFFFFFFFFFFFFFull) ? "nan" : "inf", 3);
		p += 3;
		*p = '\0';
		return p;
	}

	if (precision < 0) precision = 6;
	if (precision > RG_SPRINTF_FLOAT_MAX_PRECISION) precision = RG_SPRINTF_FLOAT_MAX_PRECISION;
	p = rg_dtoa_fixed_exact(value, p, precision);
	*p = '\0';
	return p;
}

RGINLINE char* rg_ftoa(float value, char* buf, int precision)
{
	return rg_dtoa((double)value, buf, precision);
}

// %g writes the digits rounded to 'sig' significant figures in %e form first;
// this rewrites "d.ddd" with decimal exponent exp10 (-4 <= exp10 < sig) as a
// plain decimal in place.
RGINLINE char* rg_sprintf_sci_to_fixed(char* buf, int sig, int exp10)
{
	// buf holds d.ddd (sig digits, sig - 1 after the point) or a lone digit.
	if (sig == 1)
	{
		// A single digit: "d" becomes "d" or "0.00d".
		if (exp10 >= 0) return buf + 1;
		char digit = buf[0];
		buf[0] = '0';
		buf[1] = '.';
		memset(buf + 2, '0', (size_t)(-exp10 - 1));
		buf[2 + (-exp10 - 1)] = digit;
		return buf + 2 + (-exp10);
	}
	if (exp10 >= 0)
	{
		// Slide the point right past exp10 fraction digits.
		memmove(buf + 1, buf + 2, (size_t)exp10);
		if (exp10 == sig - 1) return buf + sig; // every digit is integral
		buf[exp10 + 1] = '.';
		return buf + sig + 1;
	}
	// Move all digits right behind "0." and the leading zeros.
	int zeros = -exp10 - 1;
	memmove(buf + 2 + zeros + 1, buf + 2, (size_t)(sig - 1));
	buf[2 + zeros] = buf[0];
	buf[0] = '0';
	buf[1] = '.';
	memset(buf + 2, '0', (size_t)zeros);
	return buf + 2 + zeros + sig;
}

RGINLINE char* rg_trim_float_zeros(char* begin, char* end)
{
	char* exponent = end;
	char* decimal = NULL;

	for (char* p = begin; p < end; p++)
	{
		if (*p == '.')
		{
			decimal = p;
		}
		else if (*p == 'e' || *p == 'E')
		{
			exponent = p;
			break;
		}
	}

	if (!decimal || decimal >= exponent)
	{
		return end;
	}

	char* trimmed = exponent;
	while (trimmed > decimal + 1 && trimmed[-1] == '0')
	{
		trimmed--;
	}
	if (trimmed == decimal + 1 && trimmed[-1] == '0')
	{
		trimmed--;
	}
	if (trimmed == decimal + 1)
	{
		trimmed--;
	}

	if (exponent < end)
	{
		size_t exponent_len = (size_t)(end - exponent);
		memmove(trimmed, exponent, exponent_len);
		trimmed += exponent_len;
	}

	*trimmed = '\0';
	return trimmed;
}

// -----------------------------------------------------------------------------
// sprintf Core
// -----------------------------------------------------------------------------

// Internal sprintf context
typedef struct
{
	char* buf;
	char* end;
	size_t count;
	rg_sprintf_callback callback;
	void* user;
	int has_limit;
	char tmp[512]; // Temp buffer for callback mode
} rg_sprintf_ctx;

// Flush callback buffer
RGINLINE void rg_flush(rg_sprintf_ctx* ctx)
{
	if (ctx->callback && ctx->buf > ctx->tmp)
	{
		ptrdiff_t len = ctx->buf - ctx->tmp;
		ctx->callback(ctx->tmp, ctx->user, (int)len);
		ctx->buf = ctx->tmp;
	}
}

// Check the first eight bytes individually, which covers most identifiers and
// labels without a call; longer strings go to the platform strlen, which
// streams the rest with wide loads.
RGINLINE size_t rg_strlen(const char* s)
{
	if (s[0] == '\0') return 0;
	if (s[1] == '\0') return 1;
	if (s[2] == '\0') return 2;
	if (s[3] == '\0') return 3;
	if (s[4] == '\0') return 4;
	if (s[5] == '\0') return 5;
	if (s[6] == '\0') return 6;
	if (s[7] == '\0') return 7;
	return 8 + strlen(s + 8);
}

// Length of a string slice: at most `limit` bytes are examined, and the scan
// stops at the first null (C11 7.24.5.1), so unterminated slices are safe.
RGINLINE size_t rg_sprintf_bounded_len(const char* s, size_t limit)
{
	const char* nul = (const char*)memchr(s, '\0', limit);
	return nul ? (size_t)(nul - s) : limit;
}

// Output a single character
RGINLINE void rg_putc(rg_sprintf_ctx* ctx, char c)
{
	if (ctx->callback)
	{
		if (ctx->buf >= ctx->tmp + sizeof(ctx->tmp) - 1)
		{
			rg_flush(ctx);
		}
		*ctx->buf++ = c;
	}
	else if (!ctx->has_limit)
	{
		*ctx->buf++ = c;
	}
	else if (ctx->buf < ctx->end)
	{
		*ctx->buf++ = c;
	}
	ctx->count++;
}

// Output a string - optimized bulk copy
RGINLINE void rg_puts(rg_sprintf_ctx* ctx, const char* s, size_t len)
{
	if (ctx->callback)
	{
		// Callback mode: may need to flush
		while (len > 0)
		{
			size_t avail = sizeof(ctx->tmp) - (size_t)(ctx->buf - ctx->tmp);
			if (avail == 0)
			{
				rg_flush(ctx);
				avail = sizeof(ctx->tmp);
			}
			size_t chunk = (len < avail) ? len : avail;
			memcpy(ctx->buf, s, chunk);
			ctx->buf += chunk;
			s += chunk;
			len -= chunk;
			ctx->count += chunk;
		}
	}
	else if (!ctx->has_limit)
	{
		memcpy(ctx->buf, s, len);
		ctx->buf += len;
		ctx->count += len;
	}
	else
	{
		// Direct buffer mode: fast path
		size_t avail = (size_t)(ctx->end - ctx->buf);
		size_t to_copy = (len < avail) ? len : avail;
		memcpy(ctx->buf, s, to_copy);
		ctx->buf += to_copy;
		ctx->count += len; // Count full length even if truncated
	}
}

// Output padding - optimized for common case
RGINLINE void rg_pad(rg_sprintf_ctx* ctx, char c, int count)
{
	if (count <= 0) return;

	if (ctx->callback)
	{
		while (count > 0)
		{
			rg_putc(ctx, c);
			count--;
		}
	}
	else if (!ctx->has_limit)
	{
		memset(ctx->buf, c, (size_t)count);
		ctx->buf += count;
		ctx->count += count;
	}
	else
	{
		// Direct buffer mode: fast path
		size_t avail = (size_t)(ctx->end - ctx->buf);
		size_t to_write = ((size_t)count < avail) ? (size_t)count : avail;
		memset(ctx->buf, c, to_write);
		ctx->buf += to_write;
		ctx->count += count;
	}
}

// Format flags
#define RG_FLAG_LEFT (1 << 0)
#define RG_FLAG_PLUS (1 << 1)
#define RG_FLAG_SPACE (1 << 2)
#define RG_FLAG_HASH (1 << 3)
#define RG_FLAG_ZERO (1 << 4)
#define RG_FLAG_LONG (1 << 5)
#define RG_FLAG_LLONG (1 << 6)

// "inf"/"nan" follow the conversion's case and are never zero padded.
RGINLINE void rg_sprintf_special_case(char* buf, int uppercase, int* flags)
{
	if (uppercase)
	{
		buf[0] = (char)(buf[0] - 'a' + 'A');
		buf[1] = (char)(buf[1] - 'a' + 'A');
		buf[2] = (char)(buf[2] - 'a' + 'A');
	}
	*flags &= ~RG_FLAG_ZERO;
}


// Secure mode limits (only enforced with RG_SPRINTF_SECURE)
#ifdef RG_SPRINTF_SECURE
#define RG_MAX_WIDTH 4096
#define RG_MAX_PRECISION 1024
#endif

// Core vsnprintf implementation
static int rg_vsnprintf_internal(rg_sprintf_ctx* ctx, const char* fmt, va_list args)
{
	ctx->count = 0;

	if (!ctx->callback && !ctx->has_limit &&
	    fmt[0] == '%' && fmt[1] == 's' && fmt[2] == '\0')
	{
		char* str = va_arg(args, char*);
		if (!str) str = "(null)";

		// Tiny strings are common in logs/UI; avoid separate strlen + memcpy.
		char* out = ctx->buf;
		size_t len = 0;
		int done = 0;
		for (; len < 16; len++)
		{
			char c = str[len];
			out[len] = c;
			if (c == '\0')
			{
				done = 1;
				break;
			}
		}
		if (!done)
		{
			size_t tail_len = rg_strlen(str + 16);
			memcpy(out + 16, str + 16, tail_len);
			len += tail_len;
		}

		ctx->buf += len;
		ctx->count = len;
		return (int)ctx->count;
	}

	if (!ctx->has_limit && fmt[0] == '%' && fmt[2] == '\0')
	{
		if (fmt[1] == 'd' || fmt[1] == 'i')
		{
			int val = va_arg(args, int);
			char* start = ctx->buf;
			char* end = rg_itoa((int32_t)val, ctx->buf);
			ctx->buf = end;
			ctx->count = (size_t)(end - start);
			return (int)ctx->count;
		}

		if (fmt[1] == 'u')
		{
			unsigned int val = va_arg(args, unsigned int);
			char* start = ctx->buf;
			char* end = rg_utoa((uint32_t)val, ctx->buf);
			ctx->buf = end;
			ctx->count = (size_t)(end - start);
			return (int)ctx->count;
		}

		if (fmt[1] == 'x')
		{
			uint32_t val = va_arg(args, uint32_t);
			char tmp[16];
			char* p = tmp + sizeof(tmp) - 1;
			*p = '\0';
			if (val == 0)
			{
				*--p = '0';
			}
			else
			{
				while (val)
				{
					*--p = rg_hex_lower[val & 0xF];
					val >>= 4;
				}
			}

			size_t len = (size_t)((tmp + sizeof(tmp) - 1) - p);
			memcpy(ctx->buf, p, len);
			ctx->buf += len;
			ctx->count = len;
			return (int)ctx->count;
		}

		if (fmt[1] == 'o')
		{
			uint32_t val = va_arg(args, uint32_t);
			char tmp[16];
			char* p = tmp + sizeof(tmp) - 1;
			*p = '\0';
			if (val == 0)
			{
				*--p = '0';
			}
			else
			{
				while (val)
				{
					*--p = (char)('0' + (val & 7));
					val >>= 3;
				}
			}

			size_t len = (size_t)((tmp + sizeof(tmp) - 1) - p);
			memcpy(ctx->buf, p, len);
			ctx->buf += len;
			ctx->count = len;
			return (int)ctx->count;
		}
	}

	if (!ctx->callback && !ctx->has_limit && fmt[0] == '%' && fmt[1] == '1' &&
	    fmt[2] == '0' && fmt[3] == 's' && fmt[4] == '\0')
	{
		char* str = va_arg(args, char*);
		if (!str) str = "(null)";
		size_t len = 0;
		while (len < 10 && str[len] != '\0')
		{
			len++;
		}
		if (len == 10 && str[10] != '\0')
		{
			len += rg_strlen(str + 10);
		}

		char* out = ctx->buf;
		if (len >= 10)
		{
			memcpy(out, str, len);
			ctx->buf += len;
			ctx->count = len;
			return (int)ctx->count;
		}

		int pad = 10 - (int)len;
		memset(out, ' ', (size_t)pad);
		memcpy(out + pad, str, len);
		ctx->buf += 10;
		ctx->count = 10;
		return (int)ctx->count;
	}

	if (!ctx->callback && !ctx->has_limit && fmt[0] == '%' && fmt[1] == '-' &&
	    fmt[2] == '1' && fmt[3] == '0' && fmt[4] == 's' && fmt[5] == '\0')
	{
		char* str = va_arg(args, char*);
		if (!str) str = "(null)";
		size_t len = 0;
		while (len < 10 && str[len] != '\0')
		{
			len++;
		}
		if (len == 10 && str[10] != '\0')
		{
			len += rg_strlen(str + 10);
		}

		char* out = ctx->buf;
		if (len >= 10)
		{
			memcpy(out, str, len);
			ctx->buf += len;
			ctx->count = len;
			return (int)ctx->count;
		}

		memcpy(out, str, len);
		memset(out + len, ' ', 10 - len);
		ctx->buf += 10;
		ctx->count = 10;
		return (int)ctx->count;
	}

	if (!ctx->has_limit && fmt[0] == '%' && fmt[1] == '-' &&
	    fmt[2] == '8' && (fmt[3] == 'd' || fmt[3] == 'i') && fmt[4] == '\0')
	{
		int32_t val = va_arg(args, int);
		char* out = ctx->buf;
		int sign_len = 0;
		uint32_t uval;

		if (val < 0)
		{
			sign_len = 1;
			*out++ = '-';
			if (val == INT32_MIN)
			{
				uval = (uint32_t)INT32_MAX + 1;
			}
			else
			{
				uval = (uint32_t)(-val);
			}
		}
		else
		{
			uval = (uint32_t)val;
		}

		int digits = rg_count_digits_u32(uval);
		rg_utoa(uval, out);
		out += digits;

		int pad = 8 - (digits + sign_len);
		if (pad > 0)
		{
			size_t pad_len = (size_t)pad;
			memset(out, ' ', pad_len);
			out += pad_len;
		}

		ctx->count = (size_t)(out - ctx->buf);
		ctx->buf = out;
		return (int)ctx->count;
	}

	enum
	{
		RG_LEN_DEFAULT = 0,
		RG_LEN_LONG,
		RG_LEN_LLONG,
		RG_LEN_SIZE,
		RG_LEN_PTRDIFF,
		RG_LEN_INTMAX
	};

	while (*fmt)
	{
		if (*fmt != '%')
		{
			const char* run = fmt;
			do
			{
				fmt++;
			} while (*fmt != '\0' && *fmt != '%' && fmt - run < 8);
			if (*fmt != '\0' && *fmt != '%')
			{
				const char* pct = strchr(fmt, '%');
				fmt = pct ? pct : fmt + strlen(fmt);
			}
			rg_puts(ctx, run, (size_t)(fmt - run));
			continue;
		}

		fmt++; // Skip '%'

		// Handle %%
		if (*fmt == '%')
		{
			rg_putc(ctx, '%');
			fmt++;
			continue;
		}

		// Parse flags
		int flags = 0;
		int length = RG_LEN_DEFAULT;
		while (1)
		{
			switch (*fmt)
			{
				case '-':
					flags |= RG_FLAG_LEFT;
					fmt++;
					continue;
				case '+':
					flags |= RG_FLAG_PLUS;
					fmt++;
					continue;
				case ' ':
					flags |= RG_FLAG_SPACE;
					fmt++;
					continue;
				case '#':
					flags |= RG_FLAG_HASH;
					fmt++;
					continue;
				case '0':
					flags |= RG_FLAG_ZERO;
					fmt++;
					continue;
			}
			break;
		}

		// Parse width
		int width = 0;
		if (*fmt == '*')
		{
			width = va_arg(args, int);
			if (width < 0)
			{
				flags |= RG_FLAG_LEFT;
				width = -width;
			}
			fmt++;
		}
		else
		{
			while (*fmt >= '0' && *fmt <= '9')
			{
				width = width * 10 + (*fmt++ - '0');
			}
		}
#ifdef RG_SPRINTF_SECURE
		if (width > RG_MAX_WIDTH) width = RG_MAX_WIDTH;
#endif

		// Parse precision
		int precision = -1;
		if (*fmt == '.')
		{
			fmt++;
			precision = 0;
			if (*fmt == '*')
			{
				precision = va_arg(args, int);
				if (precision < 0) precision = -1;
				fmt++;
			}
			else
			{
				while (*fmt >= '0' && *fmt <= '9')
				{
					precision = precision * 10 + (*fmt++ - '0');
				}
			}
		}
#ifdef RG_SPRINTF_SECURE
		if (precision > RG_MAX_PRECISION) precision = RG_MAX_PRECISION;
#endif

		// Parse length modifier
		while (1)
		{
			switch (*fmt)
			{
				case 'h': fmt++; continue; // Ignored for simplicity
				case 'l':
					if (flags & RG_FLAG_LONG)
					{
						flags |= RG_FLAG_LLONG;
						length = RG_LEN_LLONG;
					}
					else
					{
						flags |= RG_FLAG_LONG;
						length = RG_LEN_LONG;
					}
					fmt++;
					continue;
				case 'z':
					length = RG_LEN_SIZE;
					// size_t is 64-bit on 64-bit platforms
#if SIZE_MAX > UINT32_MAX
					flags |= RG_FLAG_LLONG;
#else
					flags |= RG_FLAG_LONG;
#endif
					fmt++;
					continue;
				case 't':
					length = RG_LEN_PTRDIFF;
					// ptrdiff_t is 64-bit on 64-bit platforms
#if PTRDIFF_MAX > INT32_MAX
					flags |= RG_FLAG_LLONG;
#else
					flags |= RG_FLAG_LONG;
#endif
					fmt++;
					continue;
				case 'j':
					length = RG_LEN_INTMAX;
					flags |= RG_FLAG_LLONG;
					fmt++;
					continue;
			}
			break;
		}

		// Handle format specifier
		// Large enough for the longest %f (309 integer digits plus the maximum
		// precision) or %e output; integers are written backwards from the end.
		char num_buf[RG_SPRINTF_FLOAT_BUFFER_SIZE + 8];
		char* num_end;
		char* num_buf_end = num_buf + sizeof(num_buf) - 1;
		const char* str;
		size_t str_len;
		char sign_char = 0;
		const char* prefix = "";
		int prefix_len = 0;
		int is_integer = 0;
		int is_zero = 0;
		int is_octal = 0;

		switch (*fmt++)
		{
			case 'd':
			case 'i':
			{
				// FAST PATH: simple %d with no formatting - bypass output_number
				if (width == 0 && precision < 0 && !(flags & (RG_FLAG_PLUS | RG_FLAG_SPACE | RG_FLAG_LLONG | RG_FLAG_LONG)))
				{
					int32_t val = va_arg(args, int);
					if (val >= 0)
					{
						char* end = rg_utoa((uint32_t)val, num_buf);
						size_t len = (size_t)(end - num_buf);
						if (!ctx->has_limit || ctx->buf + len <= ctx->end)
						{
							memcpy(ctx->buf, num_buf, len);
							ctx->buf += len;
							ctx->count += len;
							continue;
						}
						// Fall through to slow path if buffer full
						str = num_buf;
						str_len = len;
						is_integer = 1;
						is_zero = (str_len == 1 && str[0] == '0');
						goto output_number;
					}
					else
					{
						// Negative - handle sign + digits in fast path
						if (val == INT32_MIN)
						{
							num_end = rg_utoa((uint32_t)INT32_MAX + 1, num_buf);
						}
						else
						{
							num_end = rg_utoa((uint32_t)(-val), num_buf);
						}
						size_t len = (size_t)(num_end - num_buf);
						if (!ctx->has_limit || ctx->buf + len + 1 <= ctx->end)
						{
							*ctx->buf++ = '-';
							memcpy(ctx->buf, num_buf, len);
							ctx->buf += len;
							ctx->count += len + 1;
							continue;
						}
						// Fall through to slow path
						sign_char = '-';
						str = num_buf;
						str_len = len;
						is_integer = 1;
						is_zero = (str_len == 1 && str[0] == '0');
						goto output_number;
					}
				}

				// FAST PATH: simple %lld with no formatting
				if (width == 0 && precision < 0 && (flags & RG_FLAG_LLONG) &&
				    !(flags & (RG_FLAG_PLUS | RG_FLAG_SPACE)))
				{
					int64_t val = va_arg(args, int64_t);
					if (val >= 0)
					{
						char* end = rg_u64toa((uint64_t)val, num_buf);
						size_t len = (size_t)(end - num_buf);
						if (!ctx->has_limit || ctx->buf + len <= ctx->end)
						{
							memcpy(ctx->buf, num_buf, len);
							ctx->buf += len;
							ctx->count += len;
							continue;
						}
						// Fall through to slow path if buffer full
						str = num_buf;
						str_len = len;
						is_integer = 1;
						is_zero = (str_len == 1 && str[0] == '0');
						goto output_number;
					}
					else
					{
						// Negative - handle sign + digits in fast path
						if (val == INT64_MIN)
						{
							num_end = rg_u64toa((uint64_t)INT64_MAX + 1, num_buf);
						}
						else
						{
							num_end = rg_u64toa((uint64_t)(-val), num_buf);
						}
						size_t len = (size_t)(num_end - num_buf);
						if (!ctx->has_limit || ctx->buf + len + 1 <= ctx->end)
						{
							*ctx->buf++ = '-';
							memcpy(ctx->buf, num_buf, len);
							ctx->buf += len;
							ctx->count += len + 1;
							continue;
						}
						// Fall through to slow path
						sign_char = '-';
						str = num_buf;
						str_len = len;
						is_integer = 1;
						is_zero = (str_len == 1 && str[0] == '0');
						goto output_number;
					}
				}

				// Use 64-bit path for long long, or for long on LP64 systems
				if ((flags & RG_FLAG_LLONG) || ((flags & RG_FLAG_LONG) && sizeof(long) == 8))
				{
					int64_t val;
					if (flags & RG_FLAG_LLONG)
					{
						val = va_arg(args, int64_t);
					}
					else
					{
						val = va_arg(args, long); // 64-bit on LP64
					}
					if (val < 0)
					{
						sign_char = '-';
						if (val == INT64_MIN)
						{
							num_end = rg_u64toa((uint64_t)INT64_MAX + 1, num_buf);
						}
						else
						{
							num_end = rg_u64toa((uint64_t)(-val), num_buf);
						}
					}
					else
					{
						if (flags & RG_FLAG_PLUS) sign_char = '+';
						else if (flags & RG_FLAG_SPACE) sign_char = ' ';
						num_end = rg_u64toa((uint64_t)val, num_buf);
					}
				}
				else
				{
					// Use fast 32-bit path for int, or long on LLP64 (Windows)
					int32_t val;
					if (flags & RG_FLAG_LONG)
					{
						val = (int32_t)va_arg(args, long); // 32-bit on Windows
					}
					else
					{
						val = va_arg(args, int);
					}
					if (val < 0)
					{
						sign_char = '-';
						if (val == INT32_MIN)
						{
							num_end = rg_utoa((uint32_t)INT32_MAX + 1, num_buf);
						}
						else
						{
							num_end = rg_utoa((uint32_t)(-val), num_buf);
						}
					}
					else
					{
						if (flags & RG_FLAG_PLUS) sign_char = '+';
						else if (flags & RG_FLAG_SPACE) sign_char = ' ';
						num_end = rg_utoa((uint32_t)val, num_buf);
					}
				}
				str = num_buf;
				str_len = num_end - num_buf;
				is_integer = 1;
				is_zero = (str_len == 1 && str[0] == '0');
				goto output_number;
			}

			case 'u':
			{
				// FAST PATH: simple %u with no formatting
				if (width == 0 && precision < 0 && !(flags & (RG_FLAG_LLONG | RG_FLAG_LONG)))
				{
					uint32_t val = va_arg(args, unsigned int);
					char* end = rg_utoa(val, num_buf);
					size_t len = (size_t)(end - num_buf);
					if (!ctx->has_limit || ctx->buf + len <= ctx->end)
					{
						memcpy(ctx->buf, num_buf, len);
						ctx->buf += len;
						ctx->count += len;
						continue;
					}
					// Fall through to slow path
					str = num_buf;
					str_len = len;
					is_integer = 1;
					is_zero = (str_len == 1 && str[0] == '0');
					goto output_number;
				}

				// Use 64-bit path for long long, or for long on LP64 systems
				if ((flags & RG_FLAG_LLONG) || ((flags & RG_FLAG_LONG) && sizeof(long) == 8))
				{
					uint64_t val;
					if (flags & RG_FLAG_LLONG)
					{
						val = va_arg(args, uint64_t);
					}
					else
					{
						val = va_arg(args, unsigned long); // 64-bit on LP64
					}
					num_end = rg_u64toa(val, num_buf);
				}
				else if (flags & RG_FLAG_LONG)
				{
					num_end = rg_utoa((uint32_t)va_arg(args, unsigned long), num_buf); // 32-bit on Windows
				}
				else
				{
					num_end = rg_utoa(va_arg(args, unsigned int), num_buf);
				}
				str = num_buf;
				str_len = num_end - num_buf;
				is_integer = 1;
				is_zero = (str_len == 1 && str[0] == '0');
				goto output_number;
			}

			case 'x':
			case 'X':
			{
				const char* hex_table = (fmt[-1] == 'X') ? rg_hex_upper : rg_hex_lower;

				// FAST PATH: simple %x with no formatting
				if (width == 0 && precision < 0 && !(flags & (RG_FLAG_HASH | RG_FLAG_LLONG | RG_FLAG_LONG)))
				{
					uint32_t uval = va_arg(args, unsigned int);
					uint32_t orig = uval;
					// Write backwards into temp buffer
					char* hp = num_buf_end;
					if (uval == 0)
					{
						*--hp = '0';
					}
					else
					{
						while (uval)
						{
							*--hp = hex_table[uval & 0xF];
							uval >>= 4;
						}
					}
					size_t len = (size_t)(num_buf_end - hp);
					if (!ctx->has_limit || ctx->buf + len <= ctx->end)
					{
						memcpy(ctx->buf, hp, len);
						ctx->buf += len;
						ctx->count += len;
						continue;
					}
					// Fall through to slow path
					str = hp;
					str_len = len;
					is_integer = 1;
					is_zero = (orig == 0);
					goto output_number;
				}

				uint64_t val;
				if (flags & RG_FLAG_LLONG) val = va_arg(args, uint64_t);
				else if (flags & RG_FLAG_LONG) val = va_arg(args, unsigned long);
				else val = va_arg(args, unsigned int);
				uint64_t orig = val;

				// Write backwards to avoid reversal
				char* p = num_buf_end;
				if (val == 0)
				{
					*--p = '0';
				}
				else
				{
					while (val)
					{
						*--p = hex_table[val & 0xF];
						val >>= 4;
					}
				}

				if ((flags & RG_FLAG_HASH) && orig != 0)
				{
					prefix = (fmt[-1] == 'X') ? "0X" : "0x";
					prefix_len = 2;
				}

				str = p;
				str_len = (size_t)(num_buf_end - p);
				is_integer = 1;
				is_zero = (orig == 0);
				goto output_number;
			}

			case 'o':
			{
				uint64_t val;
				if (flags & RG_FLAG_LLONG) val = va_arg(args, uint64_t);
				else if (flags & RG_FLAG_LONG) val = va_arg(args, unsigned long);
				else val = va_arg(args, unsigned int);
				uint64_t orig = val;

				// Write backwards to avoid reversal
				char* p = num_buf_end;
				if (val == 0)
				{
					*--p = '0';
				}
				else
				{
					while (val)
					{
						*--p = '0' + (val & 7);
						val >>= 3;
					}
				}

				if ((flags & RG_FLAG_HASH) && *p != '0')
				{
					prefix = "0";
					prefix_len = 1;
				}

				str = p;
				str_len = (size_t)(num_buf_end - p);
				is_integer = 1;
				is_zero = (orig == 0);
				is_octal = 1;
				goto output_number;
			}

			case 'p':
			{
				void* ptr = va_arg(args, void*);
				uint64_t val = (uint64_t)(uintptr_t)ptr;

				if (val == 0)
				{
					str = "(nil)";
					str_len = 5;
					goto output_string;
				}

				// Write backwards to avoid reversal
				char* p = num_buf_end;
				while (val)
				{
					*--p = rg_hex_lower[val & 0xF];
					val >>= 4;
				}

				prefix = "0x";
				prefix_len = 2;
				str = p;
				str_len = (size_t)(num_buf_end - p);
				goto output_number;
			}

			case 'f':
			case 'F':
			{
				double val = va_arg(args, double);
				if (precision < 0) precision = 6;
				rg_double_bits bits;
				bits.d = val;

				if (bits.u >> 63)
				{
					sign_char = '-';
					val = -val;
				}
				else if (flags & RG_FLAG_PLUS)
				{
					sign_char = '+';
				}
				else if (flags & RG_FLAG_SPACE)
				{
					sign_char = ' ';
				}

				num_end = rg_dtoa(val, num_buf, precision);
				if (isinf(val) || isnan(val))
					rg_sprintf_special_case(num_buf, fmt[-1] == 'F', &flags);
				else if ((flags & RG_FLAG_HASH) && precision == 0)
					*num_end++ = '.';
				str = num_buf;
				str_len = (size_t)(num_end - num_buf);
				goto output_float;
			}

			case 'e':
			case 'E':
			{
				double val = va_arg(args, double);
				if (precision < 0) precision = 6;

				rg_double_bits bits;
				bits.d = val;

				if (bits.u >> 63)
				{
					sign_char = '-';
					val = -val;
				}
				else if (flags & RG_FLAG_PLUS)
				{
					sign_char = '+';
				}
				else if (flags & RG_FLAG_SPACE)
				{
					sign_char = ' ';
				}

				if (isinf(val) || isnan(val))
				{
					num_end = rg_dtoa(val, num_buf, precision);
					rg_sprintf_special_case(num_buf, fmt[-1] == 'E' || fmt[-1] == 'G', &flags);
					str = num_buf;
					str_len = (size_t)(num_end - num_buf);
					goto output_float;
				}

				if (precision > RG_SPRINTF_FLOAT_MAX_PRECISION) precision = RG_SPRINTF_FLOAT_MAX_PRECISION;
				int exp_val;
				char* p = rg_dtoa_sci_exact(val, num_buf, precision, &exp_val);
				if ((flags & RG_FLAG_HASH) && precision == 0) *p++ = '.';

				// Write exponent
				*p++ = (fmt[-1] == 'E') ? 'E' : 'e';
				*p++ = (exp_val >= 0) ? '+' : '-';
				if (exp_val < 0) exp_val = -exp_val;
				if (exp_val < 10) *p++ = '0';
				p = rg_itoa(exp_val, p);

				str = num_buf;
				str_len = (size_t)(p - num_buf);
				goto output_float;
			}

			case 'g':
			case 'G':
			{
				double val = va_arg(args, double);
				if (precision < 0) precision = 6;
				if (precision == 0) precision = 1;

				rg_double_bits bits;
				bits.d = val;

				if (bits.u >> 63)
				{
					sign_char = '-';
					val = -val;
				}
				else if (flags & RG_FLAG_PLUS)
				{
					sign_char = '+';
				}
				else if (flags & RG_FLAG_SPACE)
				{
					sign_char = ' ';
				}

				if (isinf(val) || isnan(val))
				{
					num_end = rg_dtoa(val, num_buf, precision);
					rg_sprintf_special_case(num_buf, fmt[-1] == 'E' || fmt[-1] == 'G', &flags);
					str = num_buf;
					str_len = (size_t)(num_end - num_buf);
					goto output_float;
				}

				if (precision > RG_SPRINTF_FLOAT_MAX_PRECISION) precision = RG_SPRINTF_FLOAT_MAX_PRECISION;

				// Round to 'precision' significant digits first; the exponent after
				// rounding selects the style (C11 7.21.6.1: %e when X < -4 or X >= P).
				int exp_val;
				char* p = rg_dtoa_sci_exact(val, num_buf, precision - 1, &exp_val);
				if (exp_val < -4 || exp_val >= precision)
				{
					int exp_out = exp_val;
					if ((flags & RG_FLAG_HASH) && precision == 1) *p++ = '.';
					*p++ = (fmt[-1] == 'G') ? 'E' : 'e';
					*p++ = (exp_out >= 0) ? '+' : '-';
					if (exp_out < 0) exp_out = -exp_out;
					if (exp_out < 10) *p++ = '0';
					p = rg_itoa(exp_out, p);
				}
				else
				{
					p = rg_sprintf_sci_to_fixed(num_buf, precision, exp_val);
					// With '#' the point stays even when every digit is integral.
					if ((flags & RG_FLAG_HASH) && exp_val == precision - 1) *p++ = '.';
				}
				if (!(flags & RG_FLAG_HASH))
				{
					p = rg_trim_float_zeros(num_buf, p);
				}
				str = num_buf;
				str_len = (size_t)(p - num_buf);
				goto output_float;
			}

			case 's':
			{
				str = va_arg(args, char*);
				if (!str) str = "(null)";

				// FAST PATH: simple %s with no width/precision
				if (width == 0 && precision < 0)
				{
					if (!ctx->callback && !ctx->has_limit)
					{
						// Tiny strings are common in logs/UI; avoid separate strlen + memcpy.
						char* out = ctx->buf;
						size_t len = 0;
						int done = 0;
						for (; len < 16; len++)
						{
							char c = str[len];
							out[len] = c;
							if (c == '\0')
							{
								done = 1;
								break;
							}
						}
						if (!done)
						{
							size_t tail_len = rg_strlen(str + 16);
							memcpy(out + 16, str + 16, tail_len);
							len += tail_len;
						}

						ctx->buf += len;
						ctx->count += len;
						continue;
					}

					size_t len = 0;
					for (; len < 16 && str[len] != '\0'; len++)
					{
					}
					if (len == 16)
					{
						len += rg_strlen(str + 16);
					}
					if (ctx->callback)
					{
						rg_puts(ctx, str, len);
						continue;
					}
					if (!ctx->has_limit)
					{
						memcpy(ctx->buf, str, len);
						ctx->buf += len;
						ctx->count += len;
						continue;
					}
					size_t avail = (size_t)(ctx->end - ctx->buf);
					size_t to_copy = (len < avail) ? len : avail;
					memcpy(ctx->buf, str, to_copy);
					ctx->buf += to_copy;
					ctx->count += len;
					continue;
				}

				// Slow path with width/precision
				if (precision >= 0)
					str_len = rg_sprintf_bounded_len(str, (size_t)precision);
				else
					str_len = strlen(str);
				goto output_string;
			}

			case 'c':
			{
				num_buf[0] = (char)va_arg(args, int);
				str = num_buf;
				str_len = 1;
				goto output_string;
			}

			case 'n':
			{
				switch (length)
				{
					case RG_LEN_LONG:
					{
						long* n = va_arg(args, long*);
						if (n) *n = (long)ctx->count;
						break;
					}
					case RG_LEN_LLONG:
					{
						long long* n = va_arg(args, long long*);
						if (n) *n = (long long)ctx->count;
						break;
					}
					case RG_LEN_SIZE:
					{
						size_t* n = va_arg(args, size_t*);
						if (n) *n = (size_t)ctx->count;
						break;
					}
					case RG_LEN_PTRDIFF:
					{
						ptrdiff_t* n = va_arg(args, ptrdiff_t*);
						if (n) *n = (ptrdiff_t)ctx->count;
						break;
					}
					case RG_LEN_INTMAX:
					{
						intmax_t* n = va_arg(args, intmax_t*);
						if (n) *n = (intmax_t)ctx->count;
						break;
					}
					default:
					{
						int* n = va_arg(args, int*);
						if (n) *n = (int)ctx->count;
						break;
					}
				}
				continue;
			}

			default:
				// Unknown specifier, output as-is
				rg_putc(ctx, '%');
				rg_putc(ctx, fmt[-1]);
				continue;
		}

	output_number:
	{
		// C standard: 0 flag is ignored when precision is specified for integers
		if (precision >= 0)
		{
			flags &= ~RG_FLAG_ZERO;
		}

		if (is_integer && precision == 0 && is_zero && !(is_octal && (flags & RG_FLAG_HASH)))
		{
			str_len = 0;
		}

		int total_len = prefix_len + (int)str_len;
		if (sign_char) total_len++;

		// Apply precision for numbers (minimum digits)
		int zero_pad = 0;
		if (precision > (int)str_len)
		{
			zero_pad = precision - (int)str_len;
			total_len += zero_pad;
		}

		int pad_len = (width > total_len) ? width - total_len : 0;

		// Left padding with spaces
		if (!(flags & RG_FLAG_LEFT) && !(flags & RG_FLAG_ZERO))
		{
			rg_pad(ctx, ' ', pad_len);
		}

		// Sign
		if (sign_char) rg_putc(ctx, sign_char);

		// Prefix
		rg_puts(ctx, prefix, prefix_len);

		// Zero padding
		if (!(flags & RG_FLAG_LEFT) && (flags & RG_FLAG_ZERO))
		{
			rg_pad(ctx, '0', pad_len);
		}

		// Precision zero padding
		rg_pad(ctx, '0', zero_pad);

		// Number
		rg_puts(ctx, str, str_len);

		// Right padding
		if (flags & RG_FLAG_LEFT)
		{
			rg_pad(ctx, ' ', pad_len);
		}
		continue;
	}

	output_float:
	{
		int total_len = (int)str_len + (sign_char ? 1 : 0);
		int pad_len = (width > total_len) ? width - total_len : 0;

		if (!(flags & RG_FLAG_LEFT) && !(flags & RG_FLAG_ZERO))
		{
			rg_pad(ctx, ' ', pad_len);
		}

		if (sign_char) rg_putc(ctx, sign_char);

		if (!(flags & RG_FLAG_LEFT) && (flags & RG_FLAG_ZERO))
		{
			rg_pad(ctx, '0', pad_len);
		}

		rg_puts(ctx, str, str_len);

		if (flags & RG_FLAG_LEFT)
		{
			rg_pad(ctx, ' ', pad_len);
		}
		continue;
	}

	output_string:
	{
		int pad_len = (width > (int)str_len) ? width - (int)str_len : 0;

		// C standard: 0 flag has no effect on strings
		if (!(flags & RG_FLAG_LEFT))
		{
			rg_pad(ctx, ' ', pad_len);
		}

		rg_puts(ctx, str, str_len);

		if (flags & RG_FLAG_LEFT)
		{
			rg_pad(ctx, ' ', pad_len);
		}
		continue;
	}
	}

	// Flush any remaining callback buffer
	rg_flush(ctx);

	return (int)ctx->count;
}

static int rg_vsnprintf(char* buf, size_t count, const char* fmt, va_list args)
{
#ifdef RG_SPRINTF_SECURE
	if ((!buf && count > 0) || !fmt) return -1;
#endif
	rg_sprintf_ctx ctx;
	ctx.callback = NULL;
	ctx.user = NULL;
	ctx.has_limit = 1;

	char dummy;
	if (count == 0)
	{
		ctx.buf = &dummy;
		ctx.end = &dummy;
	}
	else
	{
		ctx.buf = buf;
		ctx.end = buf + count - 1; // Leave room for null terminator
	}

	int result = rg_vsnprintf_internal(&ctx, fmt, args);

	// Null terminate
	if (count > 0)
	{
		*ctx.buf = '\0';
	}

	return result;
}

static int rg_vsprintf(char* buf, const char* fmt, va_list args)
{
#ifdef RG_SPRINTF_SECURE
	if (!buf || !fmt) return -1;
#endif
	rg_sprintf_ctx ctx;
	ctx.buf = buf;
	ctx.end = NULL;
	ctx.callback = NULL;
	ctx.user = NULL;
	ctx.has_limit = 0;

	int result = rg_vsnprintf_internal(&ctx, fmt, args);
	*ctx.buf = '\0';

	return result;
}

static int rg_sprintf(char* buf, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int result = rg_vsprintf(buf, fmt, args);
	va_end(args);
	return result;
}

static int rg_snprintf(char* buf, size_t count, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int result = rg_vsnprintf(buf, count, fmt, args);
	va_end(args);
	return result;
}

static int rg_vsprintf_cb(rg_sprintf_callback cb, void* user, const char* fmt, va_list args)
{
#ifdef RG_SPRINTF_SECURE
	if (!cb || !fmt) return -1;
#endif
	rg_sprintf_ctx ctx;
	ctx.buf = ctx.tmp;
	ctx.end = ctx.tmp + sizeof(ctx.tmp);
	ctx.callback = cb;
	ctx.user = user;
	ctx.has_limit = 1;

	int result = rg_vsnprintf_internal(&ctx, fmt, args);
	return result;
}

static int rg_sprintf_cb(rg_sprintf_callback cb, void* user, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int result = rg_vsprintf_cb(cb, user, fmt, args);
	va_end(args);
	return result;
}

// -----------------------------------------------------------------------------
// String Builder
// -----------------------------------------------------------------------------

RGINLINE void rg_builder_init(RgBuilder* b, char* buf, size_t cap)
{
#ifdef RG_SPRINTF_SECURE
	RG_SPRINTF_ASSERT(b != NULL && "rg_builder_init: null builder");
	RG_SPRINTF_ASSERT(buf != NULL && "rg_builder_init: null buffer");
#endif
	b->buf = buf;
	b->len = 0;
	b->cap = cap;
	if (cap > 0) buf[0] = '\0';
}

RGINLINE void rg_builder_reset(RgBuilder* b)
{
	b->len = 0;
	if (b->cap > 0) b->buf[0] = '\0';
}

RGINLINE void rg_builder_append_char(RgBuilder* b, char c)
{
	if (b->len + 1 < b->cap)
	{
		b->buf[b->len++] = c;
		b->buf[b->len] = '\0';
	}
}

RGINLINE void rg_builder_appendn(RgBuilder* b, const char* str, size_t n)
{
	size_t available = (b->cap > b->len + 1) ? b->cap - b->len - 1 : 0;
	size_t to_copy = (n < available) ? n : available;

	if (to_copy > 0)
	{
		memcpy(b->buf + b->len, str, to_copy);
		b->len += to_copy;
		b->buf[b->len] = '\0';
	}
}

RGINLINE void rg_builder_append(RgBuilder* b, const char* str)
{
	rg_builder_appendn(b, str, strlen(str));
}

static void rg_builder_appendf(RgBuilder* b, const char* fmt, ...)
{
	// Nothing can be appended once only the terminator fits (or with no buffer).
	if (b->cap == 0 || b->len + 1 >= b->cap) return;
	size_t available = b->cap - b->len;

	va_list args;
	va_start(args, fmt);
	int written = rg_vsnprintf(b->buf + b->len, available, fmt, args);
	va_end(args);

	if (written > 0)
	{
		// rg_vsnprintf reports the untruncated length; the buffer holds at most
		// available - 1 characters plus the terminator.
		size_t appended = (size_t)written;
		if (appended >= available) appended = available - 1;
		b->len += appended;
	}
}

RGINLINE void rg_builder_append_int(RgBuilder* b, int64_t value)
{
	char tmp[24];
	char* end = rg_i64toa(value, tmp);
	rg_builder_appendn(b, tmp, end - tmp);
}

RGINLINE void rg_builder_append_uint(RgBuilder* b, uint64_t value)
{
	char tmp[24];
	char* end = rg_u64toa(value, tmp);
	rg_builder_appendn(b, tmp, end - tmp);
}

RGINLINE void rg_builder_append_hex(RgBuilder* b, const void* data, size_t len)
{
	size_t hex_len = len * 2;
	size_t available = (b->cap > b->len + 1) ? b->cap - b->len - 1 : 0;

	if (hex_len <= available)
	{
		rg_to_hex(data, len, b->buf + b->len, 0);
		b->len += hex_len;
	}
	else
	{
		// Partial write
		size_t bytes = available / 2;
		if (bytes > 0)
		{
			rg_to_hex(data, bytes, b->buf + b->len, 0);
			b->len += bytes * 2;
		}
	}
}

RGINLINE void rg_builder_append_float(RgBuilder* b, double value, int precision)
{
	char tmp[RG_SPRINTF_FLOAT_BUFFER_SIZE];
	char* end = rg_dtoa(value, tmp, precision);
	rg_builder_appendn(b, tmp, end - tmp);
}

RGINLINE size_t rg_builder_finish(RgBuilder* b)
{
	if (b->cap > 0 && b->len < b->cap)
	{
		b->buf[b->len] = '\0';
	}
	return b->len;
}

// End of rg_sprintf.h
#endif // RG_SPRINTF_H
