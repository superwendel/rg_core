# rg_thread

`rg_thread.h` provides threads, a mutex, a condition variable, a counting
semaphore and an auto-reset event with one API over Win32 (SRWLOCK,
CONDITION_VARIABLE, semaphore and event handles) and pthreads.

## Integration

```c
#include "rg_thread.h"

typedef struct Queue
{
	RgMutex lock;
	RgCondVar not_empty;
	int items;
} Queue;

static int consumer(void* user)
{
	Queue* q = (Queue*)user;
	rg_mutex_lock(&q->lock);
	while (q->items == 0)
		rg_cond_wait(&q->not_empty, &q->lock);
	q->items--;
	rg_mutex_unlock(&q->lock);
	return 0;
}

int main(void)
{
	Queue q = {0};
	RgThread t;
	rg_mutex_init(&q.lock);
	rg_cond_init(&q.not_empty);
	rg_thread_create(&t, consumer, &q, "consumer", 0);
	rg_mutex_lock(&q.lock);
	q.items++;
	rg_cond_signal(&q.not_empty);
	rg_mutex_unlock(&q.lock);
	rg_thread_join(&t, NULL);
	return 0;
}
```

## API

| Function | Behavior |
|---|---|
| `rg_thread_create(t, fn, user, name, stack)` | Start a thread; `name` is set where the OS supports it; stack 0 = default |
| `rg_thread_join(t, &result)` | Wait and collect the function's return value; 0 if not started |
| `rg_thread_current_id()` | OS thread id |
| `rg_mutex_init/destroy/lock/try_lock/unlock` | Non-recursive mutex (SRWLOCK / pthread_mutex) |
| `rg_cond_init/destroy/wait/wait_timeout/signal/broadcast` | Condition variable used with an RgMutex |
| `rg_sem_init(s, initial)/destroy/post/wait/wait_timeout` | Counting semaphore |
| `rg_event_init/destroy/signal/wait` | Auto-reset event: one signal releases one waiter; repeated signals coalesce |
| `rg_event_native_handle(e)` | Win32 HANDLE for `MsgWaitForMultipleObjectsEx`; NULL elsewhere |

Timeouts are milliseconds; `RG_THREAD_INFINITE` waits forever. Wait functions
return 1 when acquired, 0 on timeout.

## Configuration

None.

## Thread safety

All synchronization objects are safe to use from any thread after
initialization. Destroy them only with no waiters. `RgThread` is owned by the
creating thread; join from one thread only.

## Performance

Uncontended lock/unlock is a few nanoseconds on Win32 SRWLOCK. The POSIX
semaphore and event are built on a mutex + condvar pair, which costs a system
call per contended operation.
