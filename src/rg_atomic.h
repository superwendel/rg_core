// rg_atomic - minimal atomic operations for u32/u64/pointer
//
// Part of the Reverse Gravity (rg_) libraries.
//
// USAGE:
//   #include "rg_atomic.h"
//
//   u32 counter = 0;
//   rg_atomic_fetch_add_u32(&counter, 1);           // returns the previous value
//   u32 now = rg_atomic_inc_u32(&counter);          // returns the new value
//   if (rg_atomic_cas_u32(&counter, 2, 10)) ...    // 1 when counter was 2 and is now 10
//   rg_atomic_store_release_u32(&flag, 1);
//   if (rg_atomic_load_acquire_u32(&flag)) ...
//
// NOTES:
//   - Read-modify-write operations are sequentially consistent.
//   - load_acquire / store_release pair for publish/consume patterns.
//   - Values must be naturally aligned. Pointers are void*.
//   - MSVC uses compiler intrinsics; GCC and Clang use __atomic builtins.
//   - All functions have internal linkage and work in unity builds.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_ATOMIC_H
#define RG_ATOMIC_H

#include "rg_defs.h"

#if RG_COMPILER_MSVC
#include <intrin.h>

RGINLINE u32 rg_atomic_load_acquire_u32(const u32* ptr)
{
	u32 value = *(const volatile u32*)ptr;
	_ReadWriteBarrier();
	return value;
}
RGINLINE u64 rg_atomic_load_acquire_u64(const u64* ptr)
{
	u64 value = *(const volatile u64*)ptr;
	_ReadWriteBarrier();
	return value;
}
RGINLINE void* rg_atomic_load_acquire_ptr(void* const* ptr)
{
	void* value = *(void* const volatile*)ptr;
	_ReadWriteBarrier();
	return value;
}
RGINLINE void rg_atomic_store_release_u32(u32* ptr, u32 value)
{
	_ReadWriteBarrier();
	*(volatile u32*)ptr = value;
}
RGINLINE void rg_atomic_store_release_u64(u64* ptr, u64 value)
{
	_ReadWriteBarrier();
	*(volatile u64*)ptr = value;
}
RGINLINE void rg_atomic_store_release_ptr(void** ptr, void* value)
{
	_ReadWriteBarrier();
	*(void* volatile*)ptr = value;
}
RGINLINE u32 rg_atomic_fetch_add_u32(u32* ptr, u32 value)
{
	return (u32)_InterlockedExchangeAdd((volatile long*)ptr, (long)value);
}
RGINLINE u64 rg_atomic_fetch_add_u64(u64* ptr, u64 value)
{
	return (u64)_InterlockedExchangeAdd64((volatile __int64*)ptr, (__int64)value);
}
RGINLINE u32 rg_atomic_exchange_u32(u32* ptr, u32 value)
{
	return (u32)_InterlockedExchange((volatile long*)ptr, (long)value);
}
RGINLINE int rg_atomic_cas_u32(u32* ptr, u32 expected, u32 desired)
{
	return (u32)_InterlockedCompareExchange((volatile long*)ptr, (long)desired, (long)expected) == expected;
}
RGINLINE int rg_atomic_cas_u64(u64* ptr, u64 expected, u64 desired)
{
	return (u64)_InterlockedCompareExchange64((volatile __int64*)ptr, (__int64)desired, (__int64)expected) == expected;
}
RGINLINE int rg_atomic_cas_ptr(void** ptr, void* expected, void* desired)
{
	return _InterlockedCompareExchangePointer((void* volatile*)ptr, desired, expected) == expected;
}
RGINLINE void rg_atomic_fence(void)
{
	_ReadWriteBarrier();
#if RG_ARCH_X64 || RG_ARCH_X86
	_mm_mfence();
#else
	__dmb(0xF);
#endif
}

#elif RG_COMPILER_GCC || RG_COMPILER_CLANG

RGINLINE u32 rg_atomic_load_acquire_u32(const u32* ptr)
{
	return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}
RGINLINE u64 rg_atomic_load_acquire_u64(const u64* ptr)
{
	return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}
RGINLINE void* rg_atomic_load_acquire_ptr(void* const* ptr)
{
	return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}
RGINLINE void rg_atomic_store_release_u32(u32* ptr, u32 value)
{
	__atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}
RGINLINE void rg_atomic_store_release_u64(u64* ptr, u64 value)
{
	__atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}
RGINLINE void rg_atomic_store_release_ptr(void** ptr, void* value)
{
	__atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}
RGINLINE u32 rg_atomic_fetch_add_u32(u32* ptr, u32 value)
{
	return __atomic_fetch_add(ptr, value, __ATOMIC_SEQ_CST);
}
RGINLINE u64 rg_atomic_fetch_add_u64(u64* ptr, u64 value)
{
	return __atomic_fetch_add(ptr, value, __ATOMIC_SEQ_CST);
}
RGINLINE u32 rg_atomic_exchange_u32(u32* ptr, u32 value)
{
	return __atomic_exchange_n(ptr, value, __ATOMIC_SEQ_CST);
}
RGINLINE int rg_atomic_cas_u32(u32* ptr, u32 expected, u32 desired)
{
	return __atomic_compare_exchange_n(ptr, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
RGINLINE int rg_atomic_cas_u64(u64* ptr, u64 expected, u64 desired)
{
	return __atomic_compare_exchange_n(ptr, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
RGINLINE int rg_atomic_cas_ptr(void** ptr, void* expected, void* desired)
{
	return __atomic_compare_exchange_n(ptr, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
RGINLINE void rg_atomic_fence(void)
{
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
}

#else
#error "rg_atomic.h: unsupported compiler"
#endif

/** Increment and return the new value. */
RGINLINE u32 rg_atomic_inc_u32(u32* ptr)
{
	return rg_atomic_fetch_add_u32(ptr, 1u) + 1u;
}

/** Decrement and return the new value. */
RGINLINE u32 rg_atomic_dec_u32(u32* ptr)
{
	return rg_atomic_fetch_add_u32(ptr, (u32)-1) - 1u;
}

RGINLINE u64 rg_atomic_inc_u64(u64* ptr)
{
	return rg_atomic_fetch_add_u64(ptr, 1u) + 1u;
}

RGINLINE u64 rg_atomic_dec_u64(u64* ptr)
{
	return rg_atomic_fetch_add_u64(ptr, (u64)-1) - 1u;
}

#endif // RG_ATOMIC_H
