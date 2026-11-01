// rg_atomic correctness tests: single-thread semantics and a contended counter.

#include "../src/rg_atomic.h"
#include "../src/rg_atomic.h"

#include <stdio.h>
#include <string.h>

#if RG_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

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

static void test_single_thread(void)
{
	u32 a = 5u;
	u64 b = 10u;
	int dummy = 0;
	void* p = NULL;

	CHECK(rg_atomic_load_acquire_u32(&a) == 5u);
	rg_atomic_store_release_u32(&a, 7u);
	CHECK(a == 7u);
	CHECK(rg_atomic_fetch_add_u32(&a, 3u) == 7u);
	CHECK(a == 10u);
	CHECK(rg_atomic_inc_u32(&a) == 11u);
	CHECK(rg_atomic_dec_u32(&a) == 10u);
	CHECK(rg_atomic_exchange_u32(&a, 1u) == 10u);
	CHECK(a == 1u);
	CHECK(rg_atomic_cas_u32(&a, 1u, 2u) == 1);
	CHECK(a == 2u);
	CHECK(rg_atomic_cas_u32(&a, 1u, 3u) == 0);
	CHECK(a == 2u);

	CHECK(rg_atomic_load_acquire_u64(&b) == 10u);
	rg_atomic_store_release_u64(&b, 1ull << 40);
	CHECK(b == (1ull << 40));
	CHECK(rg_atomic_fetch_add_u64(&b, 1u) == (1ull << 40));
	CHECK(rg_atomic_inc_u64(&b) == (1ull << 40) + 2u);
	CHECK(rg_atomic_dec_u64(&b) == (1ull << 40) + 1u);
	CHECK(rg_atomic_cas_u64(&b, (1ull << 40) + 1u, 0u) == 1);
	CHECK(b == 0u);

	CHECK(rg_atomic_load_acquire_ptr(&p) == NULL);
	rg_atomic_store_release_ptr(&p, &dummy);
	CHECK(p == &dummy);
	CHECK(rg_atomic_cas_ptr(&p, &dummy, NULL) == 1);
	CHECK(p == NULL);
	CHECK(rg_atomic_cas_ptr(&p, &dummy, NULL) == 0);

	/* Wraparound of the decrement helper. */
	a = 0u;
	CHECK(rg_atomic_dec_u32(&a) == 0xFFFFFFFFu);
	rg_atomic_fence();
}

#define CONTENDED_THREADS 4
#define CONTENDED_ITERATIONS 200000u

typedef struct ContendedState
{
	u32 counter32;
	u64 counter64;
	u32 cas_target;
	u32 cas_wins[CONTENDED_THREADS];
} ContendedState;

static ContendedState contended;

static void contended_body(int index)
{
	u32 i;
	for (i = 0; i < CONTENDED_ITERATIONS; i++)
	{
		rg_atomic_inc_u32(&contended.counter32);
		rg_atomic_fetch_add_u64(&contended.counter64, 2u);
		/* Each CAS increments cas_target by exactly one, retrying on contention. */
		for (;;)
		{
			u32 seen = rg_atomic_load_acquire_u32(&contended.cas_target);
			if (rg_atomic_cas_u32(&contended.cas_target, seen, seen + 1u))
			{
				contended.cas_wins[index]++;
				break;
			}
		}
	}
}

#if RG_PLATFORM_WINDOWS
static DWORD WINAPI contended_thread(LPVOID user)
{
	contended_body((int)(size_t)user);
	return 0;
}
#else
static void* contended_thread(void* user)
{
	contended_body((int)(size_t)user);
	return NULL;
}
#endif

static void test_contended(void)
{
	int i;
	u32 wins = 0;
	memset(&contended, 0, sizeof contended);
#if RG_PLATFORM_WINDOWS
	HANDLE threads[CONTENDED_THREADS];
	for (i = 0; i < CONTENDED_THREADS; i++)
		threads[i] = CreateThread(NULL, 0, contended_thread, (LPVOID)(size_t)i, 0, NULL);
	WaitForMultipleObjects(CONTENDED_THREADS, threads, TRUE, INFINITE);
	for (i = 0; i < CONTENDED_THREADS; i++) CloseHandle(threads[i]);
#else
	pthread_t threads[CONTENDED_THREADS];
	for (i = 0; i < CONTENDED_THREADS; i++)
		pthread_create(&threads[i], NULL, contended_thread, (void*)(size_t)i);
	for (i = 0; i < CONTENDED_THREADS; i++) pthread_join(threads[i], NULL);
#endif
	CHECK(contended.counter32 == CONTENDED_THREADS * CONTENDED_ITERATIONS);
	CHECK(contended.counter64 == (u64)CONTENDED_THREADS * CONTENDED_ITERATIONS * 2u);
	CHECK(contended.cas_target == CONTENDED_THREADS * CONTENDED_ITERATIONS);
	for (i = 0; i < CONTENDED_THREADS; i++) wins += contended.cas_wins[i];
	CHECK(wins == CONTENDED_THREADS * CONTENDED_ITERATIONS);
}

int main(void)
{
	test_single_thread();
	test_contended();
	printf("rg_atomic tests: %d run, %d failed\n", tests_run, tests_failed);
	return tests_failed ? 1 : 0;
}
