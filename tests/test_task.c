// rg_task tests: priority ordering, bounded submission, completion draining
// without lost wakes under load, cooperative cancellation, shutdown.

#define RG_MALLOC_ASSERT(condition) ((void)sizeof(condition))
#include "../src/rg_mem.h"
#include "../src/rg_time.h"
#include "../src/rg_atomic.h"
#include "../src/rg_task.h"
#include "../src/rg_task.h"

#include <stdio.h>
#include <string.h>

#define NL "\n"

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

typedef struct TaskState
{
	u32 executed;    /* atomic */
	u32 order[64];   /* ids in execution order (single-worker tests) */
	u32 order_count; /* atomic */
	u32 cancel_seen; /* atomic */
	RgSemaphore gate;
} TaskState;

static TaskState g_state;
static RgEvent g_wake;
static u32 g_wake_count;

static void wake_cb(void* user)
{
	(void)user;
	rg_atomic_inc_u32(&g_wake_count);
	rg_event_signal(&g_wake);
}

static void record_task(void* user, const u32* cancel)
{
	u32 id = (u32)(uintptr_t)user;
	u32 slot = rg_atomic_fetch_add_u32(&g_state.order_count, 1u);
	if (slot < 64) g_state.order[slot] = id;
	rg_atomic_inc_u32(&g_state.executed);
	if (cancel && rg_atomic_load_acquire_u32(cancel)) rg_atomic_inc_u32(&g_state.cancel_seen);
}

/* Blocks the worker until the test releases the gate. */
static void gate_task(void* user, const u32* cancel)
{
	(void)cancel;
	rg_sem_wait(&g_state.gate);
	record_task(user, NULL);
}

static void cancellable_task(void* user, const u32* cancel)
{
	u32 spins = 0;
	while (!rg_atomic_load_acquire_u32(cancel) && spins++ < 5000u) rg_time_sleep_ms(1);
	record_task(user, cancel);
}

static u32 drain_all(RgTaskPool* pool)
{
	RgTask done[64];
	u32 n, total = 0;
	while ((n = rg_task_drain_completed(pool, done, 64)) != 0) total += n;
	return total;
}

static void test_priority_and_bounds(RgArena* arena)
{
	RgTaskPool pool;
	RgTask t;
	u32 i, total = 0;
	memset(&g_state, 0, sizeof g_state);
	rg_sem_init(&g_state.gate, 0);
	/* One worker so execution order is observable; capacity 4 per priority. */
	CHECK(rg_task_pool_init(&pool, arena, 1, 3, 4, wake_cb, NULL, NULL, NULL));
	t.fn = gate_task; t.user = (void*)(uintptr_t)1; t.priority = 0; t.id = 1; t.cancel = NULL;
	CHECK(rg_task_submit(&pool, &t));
	rg_time_sleep_ms(20); /* let the worker take the gate task */
	for (i = 0; i < 4; i++)
	{
		t.fn = record_task; t.user = (void*)(uintptr_t)(200 + i); t.priority = 2; t.id = 200 + i;
		CHECK(rg_task_submit(&pool, &t));
	}
	t.user = (void*)(uintptr_t)299; t.priority = 2; t.id = 299;
	CHECK(!rg_task_submit(&pool, &t)); /* priority-2 queue is full: backpressure */
	for (i = 0; i < 3; i++)
	{
		t.user = (void*)(uintptr_t)(100 + i); t.priority = 1; t.id = 100 + i;
		CHECK(rg_task_submit(&pool, &t));
	}
	t.user = (void*)(uintptr_t)7; t.priority = 0; t.id = 7;
	CHECK(rg_task_submit(&pool, &t));
	t.priority = 5;
	CHECK(!rg_task_submit(&pool, &t)); /* no such priority */
	CHECK(rg_task_pending(&pool) == 9);
	rg_sem_post(&g_state.gate);
	while (total < 9)
	{
		rg_event_wait(&g_wake, 2000);
		total += drain_all(&pool);
	}
	CHECK(total == 9);
	CHECK(g_state.order_count == 9);
	CHECK(g_state.order[0] == 1);   /* the gate task */
	CHECK(g_state.order[1] == 7);   /* priority 0 before everything queued earlier at 1 and 2 */
	CHECK(g_state.order[2] == 100 && g_state.order[3] == 101 && g_state.order[4] == 102);
	CHECK(g_state.order[5] == 200 && g_state.order[8] == 203); /* FIFO within a priority */
	CHECK(rg_task_pending(&pool) == 0);
	rg_task_pool_shutdown(&pool);
	rg_sem_destroy(&g_state.gate);
}

static void test_load_no_lost_wakes(RgArena* arena)
{
	RgTaskPool pool;
	RgTask t;
	u32 submitted = 0, completed = 0, retries = 0, loops = 0;
	const u32 target = 20000;
	memset(&g_state, 0, sizeof g_state);
	CHECK(rg_task_pool_init(&pool, arena, 4, 1, 64, wake_cb, NULL, NULL, NULL));
	t.fn = record_task; t.priority = 0; t.cancel = NULL;
	while (completed < target)
	{
		while (submitted < target)
		{
			t.user = (void*)(uintptr_t)submitted;
			t.id = submitted;
			if (!rg_task_submit(&pool, &t))
			{
				retries++;
				break; /* full: drain first */
			}
			submitted++;
		}
		completed += drain_all(&pool);
		if (completed < target && submitted == target) rg_event_wait(&g_wake, 1000);
		else if (completed < target) rg_event_wait(&g_wake, 1);
		if (++loops > 2000000u) break; /* watchdog: a lost completion must fail, not hang */
	}
	CHECK(completed == target);
	CHECK(g_state.executed == target);
	CHECK(retries > 0); /* the bound was exercised */
	CHECK(pool.completion_overflow == 0);
	CHECK(rg_task_pending(&pool) == 0);
	rg_task_pool_shutdown(&pool);
}

static void test_cancel(RgArena* arena)
{
	RgTaskPool pool;
	RgTask t, done[4];
	u32 cancel = 0;
	u64 t0;
	memset(&g_state, 0, sizeof g_state);
	CHECK(rg_task_pool_init(&pool, arena, 1, 1, 4, wake_cb, NULL, NULL, NULL));
	t.fn = cancellable_task; t.user = (void*)(uintptr_t)5; t.priority = 0; t.id = 5; t.cancel = &cancel;
	CHECK(rg_task_submit(&pool, &t));
	rg_time_sleep_ms(30);
	t0 = rg_time_ticks();
	rg_atomic_store_release_u32(&cancel, 1);
	while (!rg_task_drain_completed(&pool, done, 4)) rg_event_wait(&g_wake, 1000);
	CHECK(rg_time_ticks_to_ms(rg_time_ticks() - t0) < 1000.0); /* responded well before the 5 s spin limit */
	CHECK(done[0].id == 5);
	CHECK(g_state.cancel_seen == 1);
	rg_task_pool_shutdown(&pool);
}

static void test_shutdown_drops_queued(RgArena* arena)
{
	RgTaskPool pool;
	RgTask t;
	u32 i;
	memset(&g_state, 0, sizeof g_state);
	rg_sem_init(&g_state.gate, 0);
	CHECK(rg_task_pool_init(&pool, arena, 1, 1, 16, wake_cb, NULL, NULL, NULL));
	t.fn = gate_task; t.user = (void*)(uintptr_t)1; t.priority = 0; t.id = 1; t.cancel = NULL;
	CHECK(rg_task_submit(&pool, &t));
	rg_time_sleep_ms(20);
	for (i = 0; i < 8; i++)
	{
		t.fn = record_task; t.user = (void*)(uintptr_t)(10 + i); t.id = 10 + i;
		CHECK(rg_task_submit(&pool, &t));
	}
	rg_sem_post(&g_state.gate); /* let the running task finish; queued ones are dropped */
	rg_task_pool_shutdown(&pool);
	CHECK(pool.dropped_on_shutdown == 8);
	CHECK(g_state.executed == 1);
	CHECK(drain_all(&pool) == 1); /* the finished task is still drainable */
	CHECK(!rg_task_submit(&pool, &t)); /* stopped pools refuse work */
	rg_sem_destroy(&g_state.gate);
}

int main(void)
{
	RgArena arena;
	setvbuf(stdout, NULL, _IONBF, 0);
	rg_time_init();
	if (rg_malloc(MB(16)) != 0) return 1;
	arena = rg_arena_create(MB(8));
	rg_event_init(&g_wake);
	printf("test_priority_and_bounds...\n");
	test_priority_and_bounds(&arena);
	printf("test_load_no_lost_wakes...\n");
	test_load_no_lost_wakes(&arena);
	printf("test_cancel...\n");
	test_cancel(&arena);
	printf("test_shutdown_drops_queued...\n");
	test_shutdown_drops_queued(&arena);
	rg_event_destroy(&g_wake);
	rg_free();
	printf("rg_task tests: %d run, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
