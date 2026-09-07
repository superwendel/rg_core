// Deliberately built without whole-program optimization. Consume outside timing.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static volatile uint64_t rg_bench_sink;

uint64_t rg_bench_consume(const void* data, size_t bytes)
{
	const unsigned char* values = (const unsigned char*)data;
	uint64_t a = 0, b = 0, c = 0, d = 0;
	size_t i = 0;
	for (; bytes - i >= 32; i += 32)
	{
		uint64_t words[4];
		memcpy(words, values + i, sizeof(words));
		a += words[0]; b += words[1]; c += words[2]; d += words[3];
	}
	uint64_t sum = a + b * 3 + c * 5 + d * 7;
	for (; i < bytes; ++i) sum = sum * 33 + values[i];
	rg_bench_sink = sum;
	return sum;
}
