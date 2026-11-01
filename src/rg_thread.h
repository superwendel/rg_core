// rg_thread - threads, mutex, condition variable, semaphore, auto-reset event
//
// Part of the Reverse Gravity (rg_) libraries.
//
// USAGE:
//   #include "rg_thread.h"
//
//   static int worker(void* user) { ... return 0; }
//   RgThread t;
//   rg_thread_create(&t, worker, user, "worker", 0);
//   rg_thread_join(&t, NULL);
//
//   RgMutex m;   rg_mutex_init(&m);   rg_mutex_lock(&m); ... rg_mutex_unlock(&m);
//   RgCondVar c; rg_cond_init(&c);    rg_cond_wait(&c, &m); rg_cond_signal(&c);
//   RgSemaphore s; rg_sem_init(&s, 0); rg_sem_post(&s); rg_sem_wait(&s);
//   RgEvent e;   rg_event_init(&e);   rg_event_signal(&e); rg_event_wait(&e, 100);
//
// NOTES:
//   - Windows: CreateThread, SRWLOCK, CONDITION_VARIABLE, semaphore and
//     auto-reset event handles. Elsewhere: pthreads (and a mutex + condvar
//     pair for the semaphore and event).
//   - RgEvent is auto-reset: one signal releases one waiter; signaling an
//     already signaled event is a no-op. rg_event_native_handle returns the
//     HANDLE on Windows so a message loop can wait on it
//     (MsgWaitForMultipleObjectsEx); NULL elsewhere.
//   - Timeouts are milliseconds; wait functions return 1 when acquired and
//     0 on timeout.
//   - Objects must be initialized before use and destroyed with no waiters.
//   - The stack size of 0 selects the platform default.
//   - All functions have internal linkage and work in unity builds.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_THREAD_H
#define RG_THREAD_H

#include "rg_defs.h"

#include <stddef.h>

#define RG_THREAD_INFINITE 0xFFFFFFFFu

typedef int (*RgThreadFn)(void* user);

#if RG_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef struct RgThread
{
	HANDLE handle;
	u32 id;
	RgThreadFn fn;
	void* user;
	int result;
} RgThread;

typedef struct RgMutex
{
	SRWLOCK lock;
} RgMutex;

typedef struct RgCondVar
{
	CONDITION_VARIABLE cond;
} RgCondVar;

typedef struct RgSemaphore
{
	HANDLE handle;
} RgSemaphore;

typedef struct RgEvent
{
	HANDLE handle;
} RgEvent;

static DWORD WINAPI rg_thread_entry(LPVOID param)
{
	RgThread* thread = (RgThread*)param;
	thread->result = thread->fn(thread->user);
	return (DWORD)thread->result;
}

RGINLINE int rg_thread_create(RgThread* thread, RgThreadFn fn, void* user, const char* name, size_t stack_size)
{
	DWORD id = 0;
	thread->fn = fn;
	thread->user = user;
	thread->result = 0;
	thread->handle = CreateThread(NULL, stack_size, rg_thread_entry, thread, 0, &id);
	thread->id = (u32)id;
	if (!thread->handle) return 0;
	if (name && name[0])
	{
		wchar_t wide[64];
		int i = 0;
		while (name[i] && i < 63)
		{
			wide[i] = (wchar_t)(unsigned char)name[i];
			i++;
		}
		wide[i] = 0;
		SetThreadDescription(thread->handle, wide);
	}
	return 1;
}

RGINLINE int rg_thread_join(RgThread* thread, int* result)
{
	if (!thread->handle) return 0;
	WaitForSingleObject(thread->handle, INFINITE);
	CloseHandle(thread->handle);
	thread->handle = NULL;
	if (result) *result = thread->result;
	return 1;
}

RGINLINE u32 rg_thread_current_id(void)
{
	return (u32)GetCurrentThreadId();
}

RGINLINE void rg_mutex_init(RgMutex* m)
{
	InitializeSRWLock(&m->lock);
}
RGINLINE void rg_mutex_destroy(RgMutex* m)
{
	(void)m;
}
RGINLINE void rg_mutex_lock(RgMutex* m)
{
	AcquireSRWLockExclusive(&m->lock);
}
RGINLINE int rg_mutex_try_lock(RgMutex* m)
{
	return TryAcquireSRWLockExclusive(&m->lock) != 0;
}
RGINLINE void rg_mutex_unlock(RgMutex* m)
{
	ReleaseSRWLockExclusive(&m->lock);
}

RGINLINE void rg_cond_init(RgCondVar* c)
{
	InitializeConditionVariable(&c->cond);
}
RGINLINE void rg_cond_destroy(RgCondVar* c)
{
	(void)c;
}
RGINLINE void rg_cond_wait(RgCondVar* c, RgMutex* m)
{
	SleepConditionVariableSRW(&c->cond, &m->lock, INFINITE, 0);
}
RGINLINE int rg_cond_wait_timeout(RgCondVar* c, RgMutex* m, u32 ms)
{
	return SleepConditionVariableSRW(&c->cond, &m->lock, ms, 0) != 0;
}
RGINLINE void rg_cond_signal(RgCondVar* c)
{
	WakeConditionVariable(&c->cond);
}
RGINLINE void rg_cond_broadcast(RgCondVar* c)
{
	WakeAllConditionVariable(&c->cond);
}

RGINLINE int rg_sem_init(RgSemaphore* s, u32 initial)
{
	s->handle = CreateSemaphoreW(NULL, (LONG)initial, 0x7FFFFFFF, NULL);
	return s->handle != NULL;
}
RGINLINE void rg_sem_destroy(RgSemaphore* s)
{
	if (s->handle) CloseHandle(s->handle);
	s->handle = NULL;
}
RGINLINE void rg_sem_post(RgSemaphore* s)
{
	ReleaseSemaphore(s->handle, 1, NULL);
}
RGINLINE int rg_sem_wait_timeout(RgSemaphore* s, u32 ms)
{
	return WaitForSingleObject(s->handle, ms) == WAIT_OBJECT_0;
}
RGINLINE void rg_sem_wait(RgSemaphore* s)
{
	WaitForSingleObject(s->handle, INFINITE);
}

RGINLINE int rg_event_init(RgEvent* e)
{
	e->handle = CreateEventW(NULL, FALSE, FALSE, NULL);
	return e->handle != NULL;
}
RGINLINE void rg_event_destroy(RgEvent* e)
{
	if (e->handle) CloseHandle(e->handle);
	e->handle = NULL;
}
RGINLINE void rg_event_signal(RgEvent* e)
{
	SetEvent(e->handle);
}
RGINLINE int rg_event_wait(RgEvent* e, u32 ms)
{
	return WaitForSingleObject(e->handle, ms) == WAIT_OBJECT_0;
}
RGINLINE void* rg_event_native_handle(RgEvent* e)
{
	return (void*)e->handle;
}

#else /* POSIX */

#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

typedef struct RgThread
{
	pthread_t handle;
	int started;
	u32 id;
	RgThreadFn fn;
	void* user;
	int result;
} RgThread;

typedef struct RgMutex
{
	pthread_mutex_t lock;
} RgMutex;

typedef struct RgCondVar
{
	pthread_cond_t cond;
} RgCondVar;

/* Semaphore and auto-reset event share one mutex + condvar implementation. */
typedef struct RgSemaphore
{
	pthread_mutex_t lock;
	pthread_cond_t cond;
	u32 count;
} RgSemaphore;

typedef struct RgEvent
{
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int signaled;
} RgEvent;

static void* rg_thread_entry(void* param)
{
	RgThread* thread = (RgThread*)param;
	thread->result = thread->fn(thread->user);
	return NULL;
}

RGINLINE int rg_thread_create(RgThread* thread, RgThreadFn fn, void* user, const char* name, size_t stack_size)
{
	pthread_attr_t attr;
	int ok;
	thread->fn = fn;
	thread->user = user;
	thread->result = 0;
	thread->started = 0;
	pthread_attr_init(&attr);
	if (stack_size) pthread_attr_setstacksize(&attr, stack_size);
	ok = pthread_create(&thread->handle, &attr, rg_thread_entry, thread) == 0;
	pthread_attr_destroy(&attr);
	if (!ok) return 0;
	thread->started = 1;
#if RG_PLATFORM_LINUX
	if (name && name[0]) pthread_setname_np(thread->handle, name);
#else
	(void)name;
#endif
	return 1;
}

RGINLINE int rg_thread_join(RgThread* thread, int* result)
{
	if (!thread->started) return 0;
	pthread_join(thread->handle, NULL);
	thread->started = 0;
	if (result) *result = thread->result;
	return 1;
}

RGINLINE u32 rg_thread_current_id(void)
{
#if RG_PLATFORM_LINUX
	return (u32)syscall(SYS_gettid);
#else
	return (u32)(uintptr_t)pthread_self();
#endif
}

RGINLINE void rg_mutex_init(RgMutex* m)
{
	pthread_mutex_init(&m->lock, NULL);
}
RGINLINE void rg_mutex_destroy(RgMutex* m)
{
	pthread_mutex_destroy(&m->lock);
}
RGINLINE void rg_mutex_lock(RgMutex* m)
{
	pthread_mutex_lock(&m->lock);
}
RGINLINE int rg_mutex_try_lock(RgMutex* m)
{
	return pthread_mutex_trylock(&m->lock) == 0;
}
RGINLINE void rg_mutex_unlock(RgMutex* m)
{
	pthread_mutex_unlock(&m->lock);
}

RGINLINE void rg_thread_deadline(struct timespec* ts, u32 ms)
{
	clock_gettime(CLOCK_REALTIME, ts);
	ts->tv_sec += ms / 1000u;
	ts->tv_nsec += (long)(ms % 1000u) * 1000000L;
	if (ts->tv_nsec >= 1000000000L)
	{
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000L;
	}
}

RGINLINE void rg_cond_init(RgCondVar* c)
{
	pthread_cond_init(&c->cond, NULL);
}
RGINLINE void rg_cond_destroy(RgCondVar* c)
{
	pthread_cond_destroy(&c->cond);
}
RGINLINE void rg_cond_wait(RgCondVar* c, RgMutex* m)
{
	pthread_cond_wait(&c->cond, &m->lock);
}
RGINLINE int rg_cond_wait_timeout(RgCondVar* c, RgMutex* m, u32 ms)
{
	struct timespec ts;
	if (ms == RG_THREAD_INFINITE)
	{
		pthread_cond_wait(&c->cond, &m->lock);
		return 1;
	}
	rg_thread_deadline(&ts, ms);
	return pthread_cond_timedwait(&c->cond, &m->lock, &ts) == 0;
}
RGINLINE void rg_cond_signal(RgCondVar* c)
{
	pthread_cond_signal(&c->cond);
}
RGINLINE void rg_cond_broadcast(RgCondVar* c)
{
	pthread_cond_broadcast(&c->cond);
}

RGINLINE int rg_sem_init(RgSemaphore* s, u32 initial)
{
	pthread_mutex_init(&s->lock, NULL);
	pthread_cond_init(&s->cond, NULL);
	s->count = initial;
	return 1;
}
RGINLINE void rg_sem_destroy(RgSemaphore* s)
{
	pthread_cond_destroy(&s->cond);
	pthread_mutex_destroy(&s->lock);
}
RGINLINE void rg_sem_post(RgSemaphore* s)
{
	pthread_mutex_lock(&s->lock);
	s->count++;
	pthread_cond_signal(&s->cond);
	pthread_mutex_unlock(&s->lock);
}
RGINLINE int rg_sem_wait_timeout(RgSemaphore* s, u32 ms)
{
	int ok = 1;
	struct timespec ts;
	if (ms != RG_THREAD_INFINITE) rg_thread_deadline(&ts, ms);
	pthread_mutex_lock(&s->lock);
	while (s->count == 0 && ok)
	{
		if (ms == RG_THREAD_INFINITE) pthread_cond_wait(&s->cond, &s->lock);
		else ok = pthread_cond_timedwait(&s->cond, &s->lock, &ts) == 0;
	}
	if (ok) s->count--;
	pthread_mutex_unlock(&s->lock);
	return ok;
}
RGINLINE void rg_sem_wait(RgSemaphore* s)
{
	rg_sem_wait_timeout(s, RG_THREAD_INFINITE);
}

RGINLINE int rg_event_init(RgEvent* e)
{
	pthread_mutex_init(&e->lock, NULL);
	pthread_cond_init(&e->cond, NULL);
	e->signaled = 0;
	return 1;
}
RGINLINE void rg_event_destroy(RgEvent* e)
{
	pthread_cond_destroy(&e->cond);
	pthread_mutex_destroy(&e->lock);
}
RGINLINE void rg_event_signal(RgEvent* e)
{
	pthread_mutex_lock(&e->lock);
	e->signaled = 1;
	pthread_cond_signal(&e->cond);
	pthread_mutex_unlock(&e->lock);
}
RGINLINE int rg_event_wait(RgEvent* e, u32 ms)
{
	int ok = 1;
	struct timespec ts;
	if (ms != RG_THREAD_INFINITE) rg_thread_deadline(&ts, ms);
	pthread_mutex_lock(&e->lock);
	while (!e->signaled && ok)
	{
		if (ms == RG_THREAD_INFINITE) pthread_cond_wait(&e->cond, &e->lock);
		else ok = pthread_cond_timedwait(&e->cond, &e->lock, &ts) == 0;
	}
	if (ok) e->signaled = 0; /* auto-reset */
	pthread_mutex_unlock(&e->lock);
	return ok;
}
RGINLINE void* rg_event_native_handle(RgEvent* e)
{
	(void)e;
	return NULL;
}

#endif

#endif // RG_THREAD_H
