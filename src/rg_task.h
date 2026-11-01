// rg_task - bounded worker pool with priority queues and a completion ring
//
// Part of the Reverse Gravity (rg_) libraries.
//
// USAGE:
//   #include "rg_task.h"
//
//   static void work(void* user, const u32* cancel) { ... }
//   static void wake(void* user) { /* e.g. rg_event_signal */ }
//
//   RgTaskPool pool;
//   rg_task_pool_init(&pool, &arena, 4 /*threads*/, 2 /*priorities*/, 256 /*queue*/, wake, NULL, NULL, NULL);
//   RgTask t = {work, user, 0 /*priority*/, 0, &cancel_flag};
//   if (!rg_task_submit(&pool, &t)) { /* full: retry later */ }
//   ...
//   RgTask done[16]; u32 n = rg_task_drain_completed(&pool, done, 16);
//   rg_task_pool_shutdown(&pool);
//
// CONTRACT:
//   - Submission and draining happen on one owner thread; workers only run
//     task functions. Both are bounded: submit returns 0 when the task's run
//     queue is full OR when the tasks submitted but not yet drained would
//     exceed the completion ring, so callers drain and retry instead of growing.
//   - A completed task is appended to the completion ring, then wake(user)
//     is called. Because submit enforces the outstanding bound, the push can
//     never fail (completion_overflow stays 0).
//   - Priorities: queue 0 runs before queue 1, and so on; FIFO within a queue.
//   - Cancellation is cooperative: workers receive the task's cancel pointer
//     and poll it; the task still completes and is drained normally.
//   - Shutdown stops accepting, lets running tasks finish (queued tasks are
//     discarded and reported in dropped_on_shutdown), joins every thread.
//   - Memory comes from the arena at init; nothing allocates afterwards.
//   - To give each worker thread-local state (scratch arenas, COM init), pass
//     thread_start / thread_start_user to rg_task_pool_init; it runs once on
//     each worker thread, with the worker index, before any task.
//   - All functions have internal linkage and work in unity builds.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_TASK_H
#define RG_TASK_H

#include "rg_defs.h"
#include "rg_mem.h"
#include "rg_thread.h"

#include <string.h>

#ifndef RG_TASK_MAX_THREADS
#define RG_TASK_MAX_THREADS 32u
#endif
#ifndef RG_TASK_MAX_PRIORITIES
#define RG_TASK_MAX_PRIORITIES 8u
#endif

typedef void (*RgTaskFn)(void* user, const u32* cancel);

typedef struct RgTask
{
	RgTaskFn fn;
	void* user;
	u32 priority;
	u32 id;            /* caller-defined; carried through to the completion */
	const u32* cancel; /* optional; polled by the task */
} RgTask;

typedef struct RgTaskQueue
{
	RgTask* items;
	u32 capacity; /* power of two */
	u32 head;
	u32 tail;
} RgTaskQueue;

typedef struct RgTaskPool RgTaskPool;

typedef struct RgTaskWorkerArgs
{
	RgTaskPool* pool;
	u32 index;
} RgTaskWorkerArgs;

struct RgTaskPool
{
	RgMutex lock;
	RgCondVar work_available;
	RgTaskQueue run[RG_TASK_MAX_PRIORITIES];
	u32 priority_count;
	RgTaskQueue completed;
	RgThread threads[RG_TASK_MAX_THREADS];
	RgTaskWorkerArgs worker_args[RG_TASK_MAX_THREADS];
	u32 thread_count;
	u32 stop;
	u32 running; /* tasks currently executing */
	u32 dropped_on_shutdown;
	u32 completion_overflow; /* always 0; nonzero means the sizing invariant broke */
	void (*wake)(void* user);
	void* wake_user;
	/* Optional: called once on each worker thread before it takes work, with
	 * the worker's index [0, thread_count). */
	void (*thread_start)(void* user, u32 index);
	void* thread_start_user;
	int initialized;
};

RGINLINE void rg_task_pool_shutdown(RgTaskPool* pool);

RGINLINE u32 rg_task_pow2(u32 v)
{
	u32 p = 1;
	while (p < v) p <<= 1;
	return p;
}

RGINLINE int rg_task_queue_init(RgTaskQueue* q, RgArena* arena, u32 capacity)
{
	q->capacity = rg_task_pow2(capacity ? capacity : 1u);
	q->head = q->tail = 0;
	q->items = RG_ARENA_PUSH_ARRAY(arena, RgTask, q->capacity);
	return q->items != NULL;
}

RGINLINE u32 rg_task_queue_count(const RgTaskQueue* q)
{
	return q->tail - q->head;
}

RGINLINE int rg_task_queue_push(RgTaskQueue* q, const RgTask* t)
{
	if (q->tail - q->head >= q->capacity) return 0;
	q->items[q->tail & (q->capacity - 1u)] = *t;
	q->tail++;
	return 1;
}

RGINLINE int rg_task_queue_pop(RgTaskQueue* q, RgTask* out)
{
	if (q->tail == q->head) return 0;
	*out = q->items[q->head & (q->capacity - 1u)];
	q->head++;
	return 1;
}

static int rg_task_worker(void* user)
{
	RgTaskWorkerArgs* args = (RgTaskWorkerArgs*)user;
	RgTaskPool* pool = args->pool;
	if (pool->thread_start) pool->thread_start(pool->thread_start_user, args->index);
	for (;;)
	{
		RgTask task;
		int have = 0;
		u32 p;
		memset(&task, 0, sizeof task);
		rg_mutex_lock(&pool->lock);
		for (;;)
		{
			for (p = 0; p < pool->priority_count && !have; p++)
				have = rg_task_queue_pop(&pool->run[p], &task);
			if (have || pool->stop) break;
			rg_cond_wait(&pool->work_available, &pool->lock);
		}
		if (!have)
		{
			rg_mutex_unlock(&pool->lock);
			return 0;
		}
		pool->running++;
		rg_mutex_unlock(&pool->lock);

		task.fn(task.user, task.cancel);

		rg_mutex_lock(&pool->lock);
		pool->running--;
		/* Cannot fail: the ring was sized for every queued plus running task. */
		if (!rg_task_queue_push(&pool->completed, &task)) pool->completion_overflow++;
		rg_mutex_unlock(&pool->lock);
		if (pool->wake) pool->wake(pool->wake_user);
	}
}

/** Create the pool. queue_capacity is per priority; the completion ring holds
 * priority_count * queue_capacity entries. Threads start immediately. */
RGINLINE int rg_task_pool_init(RgTaskPool* pool, RgArena* arena, u32 thread_count, u32 priority_count,
                               u32 queue_capacity, void (*wake)(void* user), void* wake_user,
                               void (*thread_start)(void* user, u32 index), void* thread_start_user)
{
	u32 i;
	memset(pool, 0, sizeof *pool);
	pool->thread_start = thread_start;
	pool->thread_start_user = thread_start_user;
	if (!thread_count || thread_count > RG_TASK_MAX_THREADS) return 0;
	if (!priority_count || priority_count > RG_TASK_MAX_PRIORITIES) return 0;
	pool->priority_count = priority_count;
	pool->wake = wake;
	pool->wake_user = wake_user;
	rg_mutex_init(&pool->lock);
	rg_cond_init(&pool->work_available);
	for (i = 0; i < priority_count; i++)
		if (!rg_task_queue_init(&pool->run[i], arena, queue_capacity)) return 0;
	/* Every task that can exist at once (queued in any priority or running on a
	 * thread) must fit the completion ring, or a worker would have to drop one. */
	if (!rg_task_queue_init(&pool->completed, arena, rg_task_pow2(queue_capacity) * priority_count + thread_count)) return 0;
	pool->initialized = 1;
	for (i = 0; i < thread_count; i++)
	{
		char name[32];
		name[0] = 'r'; name[1] = 'g'; name[2] = '_'; name[3] = 'w'; name[4] = 'o'; name[5] = 'r'; name[6] = 'k';
		name[7] = (char)('0' + (i / 10u) % 10u);
		name[8] = (char)('0' + i % 10u);
		name[9] = 0;
		pool->worker_args[i].pool = pool;
		pool->worker_args[i].index = i;
		if (!rg_thread_create(&pool->threads[i], rg_task_worker, &pool->worker_args[i], name, 0))
		{
			rg_task_pool_shutdown(pool);
			return 0;
		}
		pool->thread_count++;
	}
	return 1;
}

/** Owner thread. Returns 0 when the task's priority queue is full or the pool
 * is stopping; the task is not taken. */
RGINLINE int rg_task_submit(RgTaskPool* pool, const RgTask* task)
{
	int ok;
	u32 outstanding, p;
	if (!pool->initialized || task->priority >= pool->priority_count) return 0;
	rg_mutex_lock(&pool->lock);
	/* Every task submitted and not yet drained (queued, running, or finished
	 * and waiting in the completion ring) needs a completion slot eventually.
	 * Refusing here is what keeps the pool bounded: the owner drains and retries. */
	outstanding = pool->running + rg_task_queue_count(&pool->completed);
	for (p = 0; p < pool->priority_count; p++) outstanding += rg_task_queue_count(&pool->run[p]);
	ok = !pool->stop && outstanding < pool->completed.capacity &&
	     rg_task_queue_push(&pool->run[task->priority], task);
	rg_mutex_unlock(&pool->lock);
	if (ok) rg_cond_signal(&pool->work_available);
	return ok;
}

/** Owner thread. Moves up to `capacity` completed tasks into `out`. Drain until
 * it returns 0 after each wake: a wake may coalesce several completions. */
RGINLINE u32 rg_task_drain_completed(RgTaskPool* pool, RgTask* out, u32 capacity)
{
	u32 n = 0;
	if (!pool->initialized) return 0;
	rg_mutex_lock(&pool->lock);
	while (n < capacity && rg_task_queue_pop(&pool->completed, &out[n])) n++;
	rg_mutex_unlock(&pool->lock);
	return n;
}

/** Owner thread. Tasks queued and running, for diagnostics and backpressure. */
RGINLINE u32 rg_task_pending(RgTaskPool* pool)
{
	u32 n, p;
	if (!pool->initialized) return 0;
	rg_mutex_lock(&pool->lock);
	n = pool->running;
	for (p = 0; p < pool->priority_count; p++) n += rg_task_queue_count(&pool->run[p]);
	rg_mutex_unlock(&pool->lock);
	return n;
}

/** Stop accepting, discard queued tasks, wait for running ones, join threads.
 * Completed tasks still in the ring can be drained afterwards. */
RGINLINE void rg_task_pool_shutdown(RgTaskPool* pool)
{
	u32 i, p;
	if (!pool->initialized) return;
	rg_mutex_lock(&pool->lock);
	pool->stop = 1;
	for (p = 0; p < pool->priority_count; p++)
	{
		pool->dropped_on_shutdown += rg_task_queue_count(&pool->run[p]);
		pool->run[p].head = pool->run[p].tail;
	}
	rg_mutex_unlock(&pool->lock);
	rg_cond_broadcast(&pool->work_available);
	for (i = 0; i < pool->thread_count; i++) rg_thread_join(&pool->threads[i], NULL);
	pool->thread_count = 0;
}

#endif // RG_TASK_H
