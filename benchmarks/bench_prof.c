// rg_prof microbenchmarks and fixed-section profiler comparison

#define RG_PROF_ENABLED 1
#include <rg_prof.h>

#include "bench_common.h"
#include <stdlib.h>

static volatile u64 benchmark_sink;

typedef struct FixedFrame
{
	u64 frame_index;
	f32 frame_ms;
	f32 section_ms[4];
} FixedFrame;

typedef struct FixedProfiler
{
	u64 frequency;
	u64 frame_start;
	u64 frame_index;
	u64 section_start[4];
	u64 section_accum[4];
	FixedFrame frames[4096];
	u32 frame_count;
	u32 frame_write_index;
	b32 enabled;
} FixedProfiler;

static FixedProfiler fixed_profiler;

typedef struct PackedProfThread
{
	RgProf* prof;
	RgProfEvent* events;
	const char* name;
	u32 capacity;
	u32 count;
	u32 dropped;
	u32 id;
	u16 depth;
} PackedProfThread;

RGINLINE void fixed_init(FixedProfiler* prof, u64 frequency)
{
	memset(prof, 0, sizeof(*prof));
	prof->frequency = frequency;
	prof->enabled = 1;
}

RGINLINE void fixed_begin_section(FixedProfiler* prof, u32 id)
{
	if (!prof->enabled) return;
	prof->section_start[id] = rg_time_ticks();
}

RGINLINE void fixed_end_section(FixedProfiler* prof, u32 id)
{
	if (!prof->enabled) return;
	u64 start = prof->section_start[id];
	if (start == 0u) return;
	prof->section_accum[id] += rg_time_ticks() - start;
	prof->section_start[id] = 0u;
}

RGINLINE void fixed_begin_frame(FixedProfiler* prof)
{
	if (!prof->enabled) return;
	prof->frame_start = rg_time_ticks();
	memset(prof->section_accum, 0, sizeof(prof->section_accum));
	memset(prof->section_start, 0, sizeof(prof->section_start));
}

RGINLINE void fixed_end_frame(FixedProfiler* prof)
{
	if (!prof->enabled) return;
	u64 now = rg_time_ticks();
	FixedFrame* frame = &prof->frames[prof->frame_write_index];
	frame->frame_index = prof->frame_index++;
	frame->frame_ms = (f32)((now - prof->frame_start) * 1000.0 / prof->frequency);
	for (u32 i = 0; i < 4u; i++)
		frame->section_ms[i] = (f32)(prof->section_accum[i] * 1000.0 / prof->frequency);
	prof->frame_write_index = (prof->frame_write_index + 1u) % RG_ARRAY_COUNT(prof->frames);
	if (prof->frame_count < RG_ARRAY_COUNT(prof->frames)) prof->frame_count++;
}

RGINLINE void packed_event_on(PackedProfThread* thread, const char* name)
{
	u32 index = thread->count;
	if (index >= thread->capacity)
	{
		thread->dropped++;
		return;
	}
	thread->count = index + 1u;
	u64 now = rg_time_ticks();
	RgProfEvent* event = &thread->events[index];
	event->name = name;
	event->start = now;
	event->end = now;
	event->thread_id = thread->id;
	event->depth = thread->depth;
	event->type = (u8)RG_PROF_EVENT_INSTANT;
	event->reserved = 0u;
}

static f64 measure_empty(RgProf* prof, size_t iterations)
{
	uint64_t local_sink = 0;
	u64 start = rg_prof_ticks_now(prof);
	for (size_t i = 0; i < iterations; i++) local_sink += i;
	f64 elapsed = rg_prof_ticks_to_ms(prof, rg_prof_ticks_now(prof) - start);
	benchmark_sink = local_sink;
	return elapsed;
}

static f64 measure_ticks(RgProf* prof, size_t iterations)
{
	uint64_t local_sink = 0;
	u64 start = rg_prof_ticks_now(prof);
	for (size_t i = 0; i < iterations; i++) local_sink += rg_prof_ticks_now(prof);
	f64 elapsed = rg_prof_ticks_to_ms(prof, rg_prof_ticks_now(prof) - start);
	benchmark_sink = local_sink;
	return elapsed;
}

static f64 measure_event(RgProfThread* thread, size_t iterations)
{
	uint64_t local_sink = 0;
	rg_prof_thread_reset(thread);
	u64 start = rg_prof_ticks_now(thread->prof);
	for (size_t i = 0; i < iterations; i++)
	{
		rg_prof_event("Event");
		local_sink += i;
	}
	f64 elapsed = rg_prof_ticks_to_ms(thread->prof, rg_prof_ticks_now(thread->prof) - start);
	benchmark_sink = local_sink;
	return elapsed;
}

static f64 measure_event_on(RgProfThread* thread, size_t iterations)
{
	uint64_t local_sink = 0;
	rg_prof_thread_reset(thread);
	u64 start = rg_prof_ticks_now(thread->prof);
	for (size_t i = 0; i < iterations; i++)
	{
		rg_prof_event_on(thread, "Event");
		local_sink += i;
	}
	f64 elapsed = rg_prof_ticks_to_ms(thread->prof, rg_prof_ticks_now(thread->prof) - start);
	benchmark_sink = local_sink;
	return elapsed;
}

static f64 measure_scope(RgProfThread* thread, size_t iterations)
{
	uint64_t local_sink = 0;
	rg_prof_thread_reset(thread);
	u64 start = rg_prof_ticks_now(thread->prof);
	for (size_t i = 0; i < iterations; i++)
	{
		RG_PROF_SCOPE("Scope")
		{
			local_sink += i;
		}
	}
	f64 elapsed = rg_prof_ticks_to_ms(thread->prof, rg_prof_ticks_now(thread->prof) - start);
	benchmark_sink = local_sink;
	return elapsed;
}

static f64 measure_scope_on(RgProfThread* thread, size_t iterations)
{
	uint64_t local_sink = 0;
	rg_prof_thread_reset(thread);
	u64 start = rg_prof_ticks_now(thread->prof);
	for (size_t i = 0; i < iterations; i++)
	{
		RG_PROF_SCOPE_ON(thread, "Scope")
		{
			local_sink += i;
		}
	}
	f64 elapsed = rg_prof_ticks_to_ms(thread->prof, rg_prof_ticks_now(thread->prof) - start);
	benchmark_sink = local_sink;
	return elapsed;
}

static f64 measure_fixed_section(RgProf* prof, size_t iterations)
{
	uint64_t local_sink = 0;
	fixed_init(&fixed_profiler, prof->ticks_per_second);
	u64 start = rg_prof_ticks_now(prof);
	for (size_t i = 0; i < iterations; i++)
	{
		fixed_begin_section(&fixed_profiler, 0u);
		local_sink += i;
		fixed_end_section(&fixed_profiler, 0u);
	}
	f64 elapsed = rg_prof_ticks_to_ms(prof, rg_prof_ticks_now(prof) - start);
	benchmark_sink = local_sink;
	return elapsed;
}

static f64 measure_rg_frame(RgProfThread* thread, size_t iterations)
{
	u64 start = rg_prof_ticks_now(thread->prof);
	for (size_t i = 0; i < iterations; i++)
	{
		rg_prof_thread_reset(thread);
		rg_prof_frame_begin_on(thread, "Frame");
		rg_prof_frame_end_on(thread, "Frame");
	}
	return rg_prof_ticks_to_ms(thread->prof, rg_prof_ticks_now(thread->prof) - start);
}

static f64 measure_fixed_frame(RgProf* prof, size_t iterations)
{
	fixed_init(&fixed_profiler, prof->ticks_per_second);
	u64 start = rg_prof_ticks_now(prof);
	for (size_t i = 0; i < iterations; i++)
	{
		fixed_begin_frame(&fixed_profiler);
		fixed_end_frame(&fixed_profiler);
	}
	return rg_prof_ticks_to_ms(prof, rg_prof_ticks_now(prof) - start);
}

static f64 measure_history(RgProfThread* thread, RgProfHistory* history, size_t iterations)
{
	uint64_t local_sink = 0;
	rg_prof_thread_reset(thread);
	rg_prof_frame_begin_on(thread, "Frame");
	RgProfScope update = rg_prof_scope_begin_on(thread, "Update");
	rg_prof_scope_end(&update);
	RgProfScope render = rg_prof_scope_begin_on(thread, "Render");
	rg_prof_scope_end(&render);
	rg_prof_frame_end_on(thread, "Frame");

	u64 start = rg_prof_ticks_now(thread->prof);
	for (size_t i = 0; i < iterations; i++)
	{
		thread->completed_frames++;
		local_sink += rg_prof_history_capture(history, thread);
	}
	f64 elapsed = rg_prof_ticks_to_ms(thread->prof, rg_prof_ticks_now(thread->prof) - start);
	benchmark_sink = local_sink;
	return elapsed;
}

#if RG_PLATFORM_WINDOWS
typedef struct ProfWorker
{
	RgProfThread* thread;
	PackedProfThread* packed_thread;
	HANDLE start_event;
	HANDLE ready_event;
	size_t iterations;
} ProfWorker;

static DWORD WINAPI prof_worker_main(LPVOID user)
{
	ProfWorker* worker = (ProfWorker*)user;
	SetEvent(worker->ready_event);
	WaitForSingleObject(worker->start_event, INFINITE);
	if (worker->packed_thread != NULL)
	{
		for (size_t i = 0; i < worker->iterations; i++)
			packed_event_on(worker->packed_thread, "Worker");
	}
	else
	{
		rg_prof_set_thread(worker->thread);
		for (size_t i = 0; i < worker->iterations; i++) RG_PROF_EVENT("Worker");
	}
	return 0;
}

static f64 measure_two_threads(RgProf* prof, RgProfThread* a, RgProfThread* b,
                               PackedProfThread* packed_a, PackedProfThread* packed_b,
                               size_t iterations)
{
	if (packed_a != NULL)
	{
		packed_a->count = 0u;
		packed_a->dropped = 0u;
		packed_b->count = 0u;
		packed_b->dropped = 0u;
	}
	else
	{
		rg_prof_thread_reset(a);
		rg_prof_thread_reset(b);
	}
	HANDLE start_event = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (start_event == NULL) return 0.0;

	HANDLE ready[2] = {CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL)};
	if (ready[0] == NULL || ready[1] == NULL)
	{
		if (ready[0]) CloseHandle(ready[0]);
		if (ready[1]) CloseHandle(ready[1]);
		CloseHandle(start_event);
		return 0.0;
	}
	ProfWorker workers[2] = {
	    {a, packed_a, start_event, ready[0], iterations},
	    {b, packed_b, start_event, ready[1], iterations}};
	HANDLE threads[2] = {NULL, NULL};
	threads[0] = CreateThread(NULL, 0, prof_worker_main, &workers[0], 0, NULL);
	threads[1] = CreateThread(NULL, 0, prof_worker_main, &workers[1], 0, NULL);
	if (threads[0] == NULL || threads[1] == NULL)
	{
		SetEvent(start_event);
		for (u32 i = 0; i < 2u; i++)
		{
			if (threads[i] == NULL) continue;
			WaitForSingleObject(threads[i], INFINITE);
			CloseHandle(threads[i]);
		}
		CloseHandle(start_event);
		CloseHandle(ready[0]);
		CloseHandle(ready[1]);
		return 0.0;
	}

	WaitForMultipleObjects(2, ready, TRUE, INFINITE);
	u64 start = rg_prof_ticks_now(prof);
	SetEvent(start_event);
	WaitForMultipleObjects(2, threads, TRUE, INFINITE);
	u64 end = rg_prof_ticks_now(prof);
	CloseHandle(threads[0]);
	CloseHandle(threads[1]);
	CloseHandle(start_event);
	CloseHandle(ready[0]);
	CloseHandle(ready[1]);
	return rg_prof_ticks_to_ms(prof, end - start);
}
#endif

typedef struct ProfBench
{
	RgProf* prof;
	RgProfThread* main_thread;
	RgProfThread* worker_a;
	RgProfThread* worker_b;
	PackedProfThread* packed;
	RgProfHistory* history;
	size_t iterations;
	int operation;
} ProfBench;

static double sample_prof(void* context, uint64_t* checksum)
{
	ProfBench* bench = (ProfBench*)context;
	size_t n = bench->iterations;
	double ms;
	switch (bench->operation)
	{
		case 0: ms = measure_empty(bench->prof, n); break;
		case 1: ms = measure_ticks(bench->prof, n); break;
		case 2: ms = measure_event(bench->main_thread, n); break;
		case 3: ms = measure_event_on(bench->main_thread, n); break;
		case 4: ms = measure_scope(bench->main_thread, n); break;
		case 5: ms = measure_scope_on(bench->main_thread, n); break;
		case 6: ms = measure_fixed_section(bench->prof, n); break;
		case 7: ms = measure_rg_frame(bench->main_thread, n); break;
		case 8: ms = measure_fixed_frame(bench->prof, n); break;
		case 9: ms = measure_history(bench->main_thread, bench->history, n); break;
#if RG_PLATFORM_WINDOWS
		case 10: ms = measure_two_threads(bench->prof, bench->worker_a,
		    bench->worker_b, NULL, NULL, n); n *= 2; break;
		case 11: ms = measure_two_threads(bench->prof, NULL, NULL,
		    &bench->packed[1], &bench->packed[2], n); n *= 2; break;
#endif
		default: return 0.0;
	}
	*checksum = rg_bench_consume(bench->main_thread->events,
	    bench->main_thread->count * sizeof(RgProfEvent));
	if (bench->operation == 6 || bench->operation == 8)
		*checksum ^= rg_bench_consume(&fixed_profiler, sizeof(fixed_profiler));
	if (bench->operation == 9)
	{
		*checksum ^= rg_bench_consume(bench->history->frames,
		    bench->history->capacity * sizeof(RgProfFrameSample));
		*checksum ^= rg_bench_consume(bench->history->section_ms,
		    bench->history->capacity * bench->history->section_count * sizeof(f32));
	}
	if (bench->operation >= 10)
	{
		if (bench->operation == 10 && (bench->worker_a->count != bench->iterations ||
		    bench->worker_b->count != bench->iterations || bench->worker_a->dropped || bench->worker_b->dropped)) exit(1);
		if (bench->operation == 11 && (bench->packed[1].count != bench->iterations ||
		    bench->packed[2].count != bench->iterations || bench->packed[1].dropped || bench->packed[2].dropped)) exit(1);
		*checksum ^= rg_bench_consume(bench->worker_a->events, bench->iterations * sizeof(RgProfEvent));
		*checksum ^= rg_bench_consume(bench->worker_b->events, bench->iterations * sizeof(RgProfEvent));
	}
	return ms * 1000000.0 / (double)n;
}

int main(int argc, char** argv)
{
	rg_bench_init(argc, argv);
	const size_t iterations = (size_t)1u << 20u;
	if (rg_malloc(MB(192)) != 0) return 1;
	RgArena arena = rg_arena_create(MB(160));
	RgProf prof;
	if (!rg_prof_init(&prof, &arena, 3, (u32)(iterations + 64))) return 1;
	ProfBench bench = {0};
	bench.prof = &prof;
	bench.iterations = iterations;
	bench.main_thread = rg_prof_register_thread(&prof, "main");
	bench.worker_a = rg_prof_register_thread(&prof, "a");
	bench.worker_b = rg_prof_register_thread(&prof, "b");
	if (!bench.main_thread || !bench.worker_a || !bench.worker_b) return 1;
	rg_prof_set_thread(bench.main_thread);
	const char* sections[] = {"Update", "Render"};
	RgProfHistory history;
	if (!rg_prof_history_init(&history, &arena, 4096, sections, 2)) return 1;
	bench.history = &history;
	bench.packed = (PackedProfThread*)rg_arena_alloc_aligned(&arena,
	    sizeof(PackedProfThread) * 3, RG_CACHE_LINE_SIZE);
	if (!bench.packed) return 1;
	memset(bench.packed, 0, sizeof(PackedProfThread) * 3);
	bench.packed[1].prof = bench.packed[2].prof = &prof;
	bench.packed[1].events = bench.worker_a->events;
	bench.packed[2].events = bench.worker_b->events;
	bench.packed[1].capacity = bench.worker_a->capacity;
	bench.packed[2].capacity = bench.worker_b->capacity;
	bench.packed[1].id = bench.worker_a->id;
	bench.packed[2].id = bench.worker_b->id;
	const char* names[] = {"prof.sum_control", "prof.timer", "prof.event_tls",
	    "prof.event_direct", "prof.scope_tls", "prof.scope_direct", "prof.fixed_section",
	    "prof.frame", "prof.fixed_frame", "prof.history", "prof.workers_isolated",
	    "prof.workers_packed"};
	for (int i = 0; i < (int)RG_ARRAY_COUNT(names); ++i)
	{
#if !RG_PLATFORM_WINDOWS
		if (i >= 10) break;
#endif
		bench.operation = i;
		rg_bench_run(names[i], sample_prof, &bench);
	}
	rg_free();
	return 0;
}
