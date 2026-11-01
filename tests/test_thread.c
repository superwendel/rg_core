// rg_thread tests: thread results, mutex-protected counter, condvar hand-offs,
// semaphore ping-pong, auto-reset event semantics and timeouts.

#include "../src/rg_thread.h"
#include "../src/rg_thread.h"
#include "../src/rg_atomic.h"

#include <stdio.h>
#include <string.h>

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

static int return_seven(void* user)
{
	(void)user;
	return 7;
}

static void test_thread_result(void)
{
	RgThread t;
	int result = 0;
	CHECK(rg_thread_create(&t, return_seven, NULL, "seven", 0));
	CHECK(rg_thread_join(&t, &result));
	CHECK(result == 7);
	CHECK(!rg_thread_join(&t, &result)); /* already joined */
	CHECK(rg_thread_current_id() != 0);
}

typedef struct CounterState
{
	RgMutex lock;
	u64 counter;
} CounterState;

static int counter_worker(void* user)
{
	CounterState* s = (CounterState*)user;
	int i;
	for (i = 0; i < 100000; i++)
	{
		rg_mutex_lock(&s->lock);
		s->counter++;
		rg_mutex_unlock(&s->lock);
	}
	return 0;
}

static void test_mutex(void)
{
	CounterState s;
	RgThread t[4];
	int i;
	rg_mutex_init(&s.lock);
	s.counter = 0;
	for (i = 0; i < 4; i++) CHECK(rg_thread_create(&t[i], counter_worker, &s, "counter", 0));
	for (i = 0; i < 4; i++) rg_thread_join(&t[i], NULL);
	CHECK(s.counter == 400000u);
	CHECK(rg_mutex_try_lock(&s.lock));
	rg_mutex_unlock(&s.lock);
	rg_mutex_destroy(&s.lock);
}

/* Producer/consumer hand-off through a condvar: 4 consumers must receive every item. */
typedef struct HandoffState
{
	RgMutex lock;
	RgCondVar cond;
	u32 queue[64];
	u32 head, tail;
	u32 produced, consumed;
	u32 target;
	int done;
} HandoffState;

static int handoff_consumer(void* user)
{
	HandoffState* s = (HandoffState*)user;
	for (;;)
	{
		rg_mutex_lock(&s->lock);
		while (s->head == s->tail && !s->done) rg_cond_wait(&s->cond, &s->lock);
		if (s->head == s->tail && s->done)
		{
			rg_mutex_unlock(&s->lock);
			return 0;
		}
		s->head++;
		s->consumed++;
		rg_cond_broadcast(&s->cond); /* wake the producer if it waits for space */
		rg_mutex_unlock(&s->lock);
	}
}

static void test_condvar(void)
{
	HandoffState s;
	RgThread t[4];
	int i;
	memset(&s, 0, sizeof s);
	rg_mutex_init(&s.lock);
	rg_cond_init(&s.cond);
	s.target = 100000;
	for (i = 0; i < 4; i++) CHECK(rg_thread_create(&t[i], handoff_consumer, &s, "consumer", 0));
	rg_mutex_lock(&s.lock);
	while (s.produced < s.target)
	{
		while (s.tail - s.head == 64) rg_cond_wait(&s.cond, &s.lock);
		s.queue[s.tail & 63] = s.produced++;
		s.tail++;
		rg_cond_signal(&s.cond);
	}
	s.done = 1;
	rg_cond_broadcast(&s.cond);
	rg_mutex_unlock(&s.lock);
	for (i = 0; i < 4; i++) rg_thread_join(&t[i], NULL);
	CHECK(s.consumed == s.target);
	rg_cond_destroy(&s.cond);
	rg_mutex_destroy(&s.lock);
}

typedef struct PingPong
{
	RgSemaphore ping, pong;
	u32 rounds;
	u32 seen;
} PingPong;

static int pong_worker(void* user)
{
	PingPong* p = (PingPong*)user;
	u32 i;
	for (i = 0; i < p->rounds; i++)
	{
		rg_sem_wait(&p->ping);
		p->seen++;
		rg_sem_post(&p->pong);
	}
	return 0;
}

static void test_semaphore(void)
{
	PingPong p;
	RgThread t;
	u32 i;
	p.rounds = 10000;
	p.seen = 0;
	CHECK(rg_sem_init(&p.ping, 0));
	CHECK(rg_sem_init(&p.pong, 0));
	CHECK(!rg_sem_wait_timeout(&p.ping, 10)); /* nothing posted: times out */
	CHECK(rg_thread_create(&t, pong_worker, &p, "pong", 0));
	for (i = 0; i < p.rounds; i++)
	{
		rg_sem_post(&p.ping);
		CHECK(rg_sem_wait_timeout(&p.pong, 5000));
	}
	rg_thread_join(&t, NULL);
	CHECK(p.seen == p.rounds);
	rg_sem_destroy(&p.ping);
	rg_sem_destroy(&p.pong);
}

typedef struct EventState
{
	RgEvent event;
	u32 signals_sent;
	u32 wakes;
	int stop;
} EventState;

static int event_waiter(void* user)
{
	EventState* s = (EventState*)user;
	while (!rg_atomic_load_acquire_u32((u32*)&s->stop))
	{
		if (rg_event_wait(&s->event, 50)) s->wakes++;
	}
	return 0;
}

static void test_event(void)
{
	EventState s;
	RgThread t;
	memset(&s, 0, sizeof s);
	CHECK(rg_event_init(&s.event));
	CHECK(!rg_event_wait(&s.event, 10));
	rg_event_signal(&s.event);
	rg_event_signal(&s.event); /* coalesces: still one wake */
	CHECK(rg_event_wait(&s.event, 10));
	CHECK(!rg_event_wait(&s.event, 10)); /* auto-reset consumed it */
#if RG_PLATFORM_WINDOWS
	CHECK(rg_event_native_handle(&s.event) != NULL);
#else
	CHECK(rg_event_native_handle(&s.event) == NULL);
#endif
	CHECK(rg_thread_create(&t, event_waiter, &s, "waiter", 0));
	{
		u32 i;
		for (i = 0; i < 1000; i++)
		{
			rg_event_signal(&s.event);
			s.signals_sent++;
		}
	}
	rg_atomic_store_release_u32((u32*)&s.stop, 1);
	rg_event_signal(&s.event);
	rg_thread_join(&t, NULL);
	/* Signals coalesce, so wakes <= signals; but at least one wake happened. */
	CHECK(s.wakes >= 1 && s.wakes <= s.signals_sent + 1);
	rg_event_destroy(&s.event);
}

int main(void)
{
	test_thread_result();
	test_mutex();
	test_condvar();
	test_semaphore();
	test_event();
	printf("rg_thread tests: %d run, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
