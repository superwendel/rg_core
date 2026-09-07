// Isolate lifecycle costs from event pumping and rendering. SDL getters are real.
#include <rg_input.h>
#include "bench_common.h"
#include <stdlib.h>

typedef struct InputBench
{
	RgInputState* states;
	int relative;
	size_t count;
} InputBench;

static double sample_input(void* context, uint64_t* checksum)
{
	InputBench* bench = (InputBench*)context;
	const size_t count = bench->count;
	const size_t repeats = 1048576 / count;
	for (size_t i = 0; i < count; ++i)
	{
		rg_input_init(&bench->states[i]);
		bench->states[i].relative_mouse_enabled = bench->relative;
	}
	SDL_PumpEvents();
	double start = rg_bench_now_ns();
	for (size_t repeat = 0; repeat < repeats; ++repeat)
	{
		for (size_t i = 0; i < count; ++i)
		{
			rg_input_begin_frame(&bench->states[i]);
			rg_input_sample(&bench->states[i]);
		}
	}
	double elapsed = rg_bench_now_ns() - start;
	*checksum = rg_bench_consume(bench->states, count * sizeof(*bench->states));
	return elapsed / (double)(count * repeats);
}

int main(int argc, char** argv)
{
	rg_bench_init(argc, argv);
	if (!SDL_Init(SDL_INIT_EVENTS)) return 1;
	InputBench bench;
	bench.states = (RgInputState*)calloc(1024, sizeof(RgInputState));
	if (!bench.states) { SDL_Quit(); return 1; }
	bench.relative = 0;
	bench.count = 1024;
	rg_bench_run("input.absolute", sample_input, &bench);
	bench.relative = 1;
	rg_bench_run("input.relative", sample_input, &bench);
	bench.count = 1;
	bench.relative = 0;
	rg_bench_run("input.absolute_single", sample_input, &bench);
	bench.relative = 1;
	rg_bench_run("input.relative_single", sample_input, &bench);
	free(bench.states);
	SDL_Quit();
	return 0;
}
