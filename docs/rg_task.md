# rg_task

`rg_task.h` runs tasks on a fixed set of worker threads with per-priority
bounded queues and a completion ring the owner thread drains. It is the
minimum needed for "submit work, get told when it is done" without unbounded
growth or lost wake-ups.

## Integration

```c
#include "rg_task.h"

static void scan(void* user, const u32* cancel)
{
	/* ... poll *cancel between batches ... */
}

static void on_done(void* user)
{
	rg_event_signal((RgEvent*)user);
}

int main(void)
{
	RgEvent wake;
	RgTaskPool pool;
	RgArena arena = rg_arena_create(MB(1));
	RgTask task = {scan, NULL, 0 /*priority*/, 42 /*id*/, NULL /*cancel*/};
	RgTask done[16];
	u32 n;
	rg_event_init(&wake);
	rg_task_pool_init(&pool, &arena, 4, 2, 256, on_done, &wake);
	if (!rg_task_submit(&pool, &task))
		return 1; /* queue full: apply backpressure */
	rg_event_wait(&wake, RG_THREAD_INFINITE);
	while ((n = rg_task_drain_completed(&pool, done, 16)) != 0)
		;
	rg_task_pool_shutdown(&pool);
	return 0;
}
```

## API

| Function | Behavior |
|---|---|
| `rg_task_pool_init(pool, arena, threads, priorities, queue_capacity, wake, user)` | Allocate queues from the arena and start the threads |
| `rg_task_submit(pool, task)` | Owner thread; 0 when that priority's queue is full, when the tasks submitted but not yet drained would exceed the completion ring, or when the pool is stopping |
| `rg_task_drain_completed(pool, out, capacity)` | Owner thread; moves finished tasks out of the ring |
| `rg_task_pending(pool)` | Queued plus running tasks |
| `rg_task_pool_shutdown(pool)` | Stop accepting, drop queued tasks (`dropped_on_shutdown`), wait for running ones, join |

A task is `{fn, user, priority, id, cancel}`. Lower priority numbers run first;
FIFO within a priority. `cancel` is an optional pointer the task polls; the pool
never frees or interprets it.

## Configuration

| Macro | Default | Purpose |
|---|---|---|
| `RG_TASK_MAX_THREADS` | 32 | Upper bound on worker threads |
| `RG_TASK_MAX_PRIORITIES` | 8 | Upper bound on priority queues |

## Thread safety

Submission and draining belong to one owner thread. Workers touch only the
task they run. The wake callback runs on a worker thread after the completion
was published; it must be cheap and thread-safe (signal an event, post a
message). Drain until the ring is empty after every wake: several completions
can share one wake.

## Performance

Queues are power-of-two rings under one SRWLOCK/mutex; a submit or drain is a
lock, a copy and an unlock. Workers sleep on a condition variable when idle.
