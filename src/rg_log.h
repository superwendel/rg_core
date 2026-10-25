// rg_log - Fast, lightweight logging for C
//
// Part of the Reverse Gravity (rg_) core libraries.
// Single-header logger built on rg_sprintf.
//
// USAGE:
//   #include "rg_log.h"
//
//   rg_log_init(RG_LOG_DEBUG);
//   RG_INFO("Server started on port %d", 8080);
//
// OPTIONS:
//   #define RG_LOG_TIMESTAMP        - Enable timestamps (default: off)
//   #define RG_LOG_STRIP_EXTENSION  - Strip the source file extension (default: off)
//   #define RG_LOG_NO_COLOR         - Disable ANSI colors (default: colors on)
//   #define RG_LOG_BUFFER_SIZE 512  - Override the message buffer size
//
// UNITY BUILD NOTE:
//   - This header intentionally has no include guard. Include it once in a
//     unity translation unit so all source files share one logger state.
//   - Separately compiled translation units each receive independent state.
//   - Include rg_assert.h after this header to route assertion reports through
//     RG_CRIT by default.
//
// Author: Steven Wendel (superwendel)

#include "rg_sprintf_hybrid.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if defined(RG_LOG_TIMESTAMP)
#include <time.h>
#endif

#ifndef RG_LOG_BUFFER_SIZE
#define RG_LOG_BUFFER_SIZE 512
#endif

#ifndef RG_LOG_NO_COLOR
#define RG_LOG_ANSI_RESET "\x1b[0m"
#define RG_LOG_ANSI_RED "\x1b[31m"
#define RG_LOG_ANSI_GREEN "\x1b[32m"
#define RG_LOG_ANSI_YELLOW "\x1b[33m"
#define RG_LOG_ANSI_BLUE "\x1b[34m"
#define RG_LOG_ANSI_CYAN "\x1b[36m"
#define RG_LOG_ANSI_GRAY "\x1b[90m"
#else
#define RG_LOG_ANSI_RESET ""
#define RG_LOG_ANSI_RED ""
#define RG_LOG_ANSI_GREEN ""
#define RG_LOG_ANSI_YELLOW ""
#define RG_LOG_ANSI_BLUE ""
#define RG_LOG_ANSI_CYAN ""
#define RG_LOG_ANSI_GRAY ""
#endif

typedef enum RgLogLevel
{
	RG_LOG_DEBUG,
	RG_LOG_INFO,
	RG_LOG_WARN,
	RG_LOG_ERROR,
	RG_LOG_CRIT,
	RG_LOG_NONE
} RgLogLevel;

typedef struct RgLogLevelInfo
{
	const char* name;
	const char* color;
	FILE* stream;
} RgLogLevelInfo;

static RgLogLevel rg_log_level = RG_LOG_INFO;
static RgLogLevelInfo rg_log_level_table[6];
static int rg_log_initialized;

#define RG_LOG_LEVEL_INFO_INIT(level_name, level_color, level_stream) \
	(RgLogLevelInfo) { level_name, level_color, level_stream }

// Kept out of line: the level table is only needed once a message is written,
// so the per-call-site level check must not drag its initializer along.
static RG_NOINLINE void rg_log_init_defaults(void)
{
	if (rg_log_initialized)
		return;

	rg_log_level_table[RG_LOG_DEBUG] = RG_LOG_LEVEL_INFO_INIT("DBUG", RG_LOG_ANSI_BLUE, stdout);
	rg_log_level_table[RG_LOG_INFO] = RG_LOG_LEVEL_INFO_INIT("INFO", RG_LOG_ANSI_GREEN, stdout);
	rg_log_level_table[RG_LOG_WARN] = RG_LOG_LEVEL_INFO_INIT("WARN", RG_LOG_ANSI_YELLOW, stderr);
	rg_log_level_table[RG_LOG_ERROR] = RG_LOG_LEVEL_INFO_INIT("EROR", RG_LOG_ANSI_RED, stderr);
	rg_log_level_table[RG_LOG_CRIT] = RG_LOG_LEVEL_INFO_INIT("CRIT", RG_LOG_ANSI_RED, stderr);
	rg_log_level_table[RG_LOG_NONE] = RG_LOG_LEVEL_INFO_INIT("NONE", RG_LOG_ANSI_RESET, stdout);
	rg_log_initialized = 1;
}

#undef RG_LOG_LEVEL_INFO_INIT

// =============================================================================
// PUBLIC API
// =============================================================================

RGINLINE void rg_log_init(RgLogLevel level)
{
	rg_log_init_defaults();
	rg_log_level = level;
}

RGINLINE void rg_log_set_level(RgLogLevel level)
{
	rg_log_init_defaults();
	rg_log_level = level;
}

RGINLINE RgLogLevel rg_log_get_level(void)
{
	rg_log_init_defaults();
	return rg_log_level;
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

RGINLINE char* rg_log_putc(char* buf, char* end, char c)
{
	if (buf < end)
		*buf++ = c;
	return buf;
}

RGINLINE char* rg_log_puts(char* buf, char* end, const char* string)
{
	while (*string && buf < end)
		*buf++ = *string++;
	return buf;
}

#if defined(RG_LOG_TIMESTAMP)
// Formatting the local time costs about a microsecond, so each thread caches
// the text for the current second. Thread-local storage keeps the cache free
// of shared mutable state.
static RG_THREAD_LOCAL time_t rg_log_timestamp_second = (time_t)-1;
static RG_THREAD_LOCAL char rg_log_timestamp_text[9] = "00:00:00";

RGINLINE const char* rg_log_timestamp(void)
{
	time_t now = time(NULL);
	if (now == rg_log_timestamp_second)
		return rg_log_timestamp_text;

	struct tm* time_info;
#if RG_COMPILER_MSVC
	struct tm time_storage;
	time_info = localtime_s(&time_storage, &now) == 0 ? &time_storage : NULL;
#elif defined(_POSIX_C_SOURCE) || defined(_POSIX_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_DEFAULT_SOURCE) || defined(_GNU_SOURCE) || defined(_BSD_SOURCE) || RG_PLATFORM_MACOS
	struct tm time_storage;
	time_info = localtime_r(&now, &time_storage);
#else
	// Strict ISO C without a reentrant localtime; shares libc's static buffer.
	time_info = localtime(&now);
#endif
	if (time_info == NULL ||
	    strftime(rg_log_timestamp_text, sizeof(rg_log_timestamp_text), "%H:%M:%S", time_info) == 0)
	{
		memcpy(rg_log_timestamp_text, "00:00:00", sizeof(rg_log_timestamp_text));
	}
	rg_log_timestamp_second = now;
	return rg_log_timestamp_text;
}
#endif

RGINLINE char* rg_log_itoa(char* buf, char* end, int value)
{
	if (value < 0)
	{
		buf = rg_log_putc(buf, end, '-');
		value = -value;
	}

	char temp[16];
	int digits = 0;
	if (value == 0)
		temp[digits++] = '0';
	else
	{
		while (value > 0)
		{
			temp[digits++] = (char)('0' + value % 10);
			value /= 10;
		}
	}

	while (digits > 0 && buf < end)
		*buf++ = temp[--digits];
	return buf;
}

RGINLINE char* rg_log_build_prefix(char* buffer, size_t buffer_size, RgLogLevel level, const char* file, int line)
{
	rg_log_init_defaults();
	if (buffer_size <= 2)
		return buffer;

	RgLogLevelInfo* info = &rg_log_level_table[level];
	char* cursor = buffer;
	char* end = buffer + buffer_size - 2;

#if defined(RG_LOG_TIMESTAMP)
	cursor = rg_log_puts(cursor, end, RG_LOG_ANSI_GRAY);
	cursor = rg_log_putc(cursor, end, '[');
	cursor = rg_log_puts(cursor, end, rg_log_timestamp());
	cursor = rg_log_putc(cursor, end, ']');
	cursor = rg_log_putc(cursor, end, ' ');
#endif

	cursor = rg_log_puts(cursor, end, info->color);
	cursor = rg_log_putc(cursor, end, '[');
	cursor = rg_log_puts(cursor, end, info->name);
	cursor = rg_log_putc(cursor, end, ']');
	cursor = rg_log_puts(cursor, end, RG_LOG_ANSI_RESET);
	cursor = rg_log_putc(cursor, end, ' ');
	cursor = rg_log_puts(cursor, end, RG_LOG_ANSI_CYAN);

	const char* filename = file;
	const char* last_slash = strrchr(file, '/');
	const char* last_backslash = strrchr(file, '\\');
	if (last_slash != NULL && last_slash > filename)
		filename = last_slash + 1;
	if (last_backslash != NULL && last_backslash > filename)
		filename = last_backslash + 1;

#if defined(RG_LOG_STRIP_EXTENSION)
	const char* dot = strrchr(filename, '.');
	while (*filename && filename != dot && cursor < end)
		*cursor++ = *filename++;
#else
	cursor = rg_log_puts(cursor, end, filename);
#endif

	cursor = rg_log_putc(cursor, end, ':');
	cursor = rg_log_itoa(cursor, end, line);
	cursor = rg_log_puts(cursor, end, RG_LOG_ANSI_RESET);
	cursor = rg_log_putc(cursor, end, ':');
	cursor = rg_log_putc(cursor, end, ' ');
	return cursor;
}

static void rg_log(RgLogLevel level, const char* file, int line, const char* format, ...)
{
#if RG_LOG_BUFFER_SIZE <= 2
	RG_UNUSED(level);
	RG_UNUSED(file);
	RG_UNUSED(line);
	RG_UNUSED(format);
#else
	if ((unsigned)level >= (unsigned)RG_LOG_NONE || level < rg_log_level)
		return;

	char log_buffer[RG_LOG_BUFFER_SIZE];
	char* cursor = rg_log_build_prefix(log_buffer, (size_t)RG_LOG_BUFFER_SIZE, level, file, line);
	size_t remaining = (size_t)RG_LOG_BUFFER_SIZE - (size_t)(cursor - log_buffer) - 2;

	va_list args;
	va_start(args, format);
	int written = rg_vsnprintf(cursor, remaining + 1, format, args);
	va_end(args);

	if (written > 0)
	{
		size_t clamped = (size_t)written < remaining ? (size_t)written : remaining;
		cursor += clamped;
	}
	*cursor++ = '\n';
	*cursor = '\0';

	fwrite(log_buffer, 1, (size_t)(cursor - log_buffer), rg_log_level_table[level].stream);
	if (level >= RG_LOG_ERROR)
		fflush(rg_log_level_table[level].stream);
#endif
}

// The macro-level check prevents formatting arguments from being evaluated
// when a message is filtered out. It compares against the level variable
// directly; rg_log initializes the level table before using it, so the lazy
// initialization stays off the call site.
#define RG_LOG_INTERNAL_WRITE(level, ...)                     \
	do                                                         \
	{                                                          \
		if (RG_UNLIKELY((level) >= rg_log_level))                \
			rg_log((level), __FILE__, __LINE__, __VA_ARGS__);     \
	} while (0)

#define RG_DEBUG(...) RG_LOG_INTERNAL_WRITE(RG_LOG_DEBUG, __VA_ARGS__)
#define RG_INFO(...) RG_LOG_INTERNAL_WRITE(RG_LOG_INFO, __VA_ARGS__)
#define RG_WARN(...) RG_LOG_INTERNAL_WRITE(RG_LOG_WARN, __VA_ARGS__)
#define RG_ERROR(...) RG_LOG_INTERNAL_WRITE(RG_LOG_ERROR, __VA_ARGS__)
#define RG_CRIT(...) RG_LOG_INTERNAL_WRITE(RG_LOG_CRIT, __VA_ARGS__)
