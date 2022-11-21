


#include "SpinLocks.h"

#include <stdlib.h>
#include <stdint.h>
#include <immintrin.h>
#include <time.h>
#include <sched.h>


// TAS lock 


const unsigned TAS_CYCLES_TO_SPIN          =    10;
const unsigned TAS_MIN_BACKOFF_NANOSECONDS =  1000;
const unsigned TAS_MAX_BACKOFF_NANOSECONDS = 64000;

void TAS_init(struct TAS_Lock* lock)
{
	lock->lock_taken = 0;
}

void TAS_acquire(struct TAS_Lock* lock)
{
	unsigned backoff_sleep = TAS_MIN_BACKOFF_NANOSECONDS;
/*
Built-in Function: bool __atomic_test_and_set (void *ptr, int memorder)
This built-in function performs an atomic test-and-set operation on the byte at *ptr. 
The byte is set to some implementation defined nonzero “set” value and the return value is true if and only if the previous contents were “set”. 
It should be only used for operands of type bool or char. For other types only part of the value may be set.

All memory orders are valid.
*/
	for (unsigned cycle_no = 0; __atomic_test_and_set(&lock->lock_taken, __ATOMIC_ACQUIRE); ++cycle_no)
	{
		spinloop_pause();

		if (cycle_no == TAS_CYCLES_TO_SPIN)
		{
			struct timespec to_sleep = {
				.tv_sec  = 0,
				.tv_nsec = backoff_sleep + (rand() % TAS_MIN_BACKOFF_NANOSECONDS)
			};

			if (backoff_sleep < TAS_MAX_BACKOFF_NANOSECONDS) backoff_sleep *= 2;
			cycle_no = TAS_CYCLES_TO_SPIN - 1;

			// No value-checking, because it doesn't affect correctness
			nanosleep(&to_sleep, NULL);
		}
	}

	backoff_sleep = TAS_MIN_BACKOFF_NANOSECONDS;
}

void TAS_release(struct TAS_Lock* lock)
{
	__atomic_clear(&lock->lock_taken, __ATOMIC_RELEASE);
}
/*
Built-in Function: void __atomic_clear (bool *ptr, int memorder)
This built-in function performs an atomic clear operation on *ptr. After the operation, *ptr contains 0. 
It should be only used for operands of type bool or char and in conjunction with __atomic_test_and_set. 
For other types it may only clear partially. If the type is not bool prefer using __atomic_store.

The valid memory order variants are __ATOMIC_RELAXED, __ATOMIC_SEQ_CST, and __ATOMIC_RELEASE.
*/
// TTAS lock 


const unsigned TTAS_CYCLES_TO_SPIN          =    10;
const unsigned TTAS_MIN_BACKOFF_NANOSECONDS =  1000;
const unsigned TTAS_MAX_BACKOFF_NANOSECONDS = 64000;

void TTAS_init(struct TTAS_Lock* lock)
{
	lock->lock_taken = 0;
}

void TTAS_acquire(struct TTAS_Lock* lock)
{
	unsigned backoff_sleep = TTAS_MIN_BACKOFF_NANOSECONDS;

	// On start spin-loop waiting for the lock to be released:
	/*
	Built-in Function: type __atomic_load_n (type *ptr, int memorder)
This built-in function implements an atomic load operation. It returns the contents of *ptr.

The valid memory order variants are __ATOMIC_RELAXED, __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE, and __ATOMIC_CONSUME.

Built-in Function: void __atomic_load (type *ptr, type *ret, int memorder)
This is the generic version of an atomic load. It returns the contents of *ptr in *ret.


	*/
	for (unsigned cycle_no = 0; __atomic_load_n(&lock->lock_taken, __ATOMIC_SEQ_CST) && cycle_no < TTAS_CYCLES_TO_SPIN; ++cycle_no)
	{
		spinloop_pause();
	}

	// Perform exponential backoff:
	while (1)
	{
		if (__atomic_load_n(&lock->lock_taken, __ATOMIC_SEQ_CST))
		{
			struct timespec to_sleep = {
				.tv_sec  = 0,
				.tv_nsec = backoff_sleep + (rand() % TTAS_MIN_BACKOFF_NANOSECONDS)
			};

			if (backoff_sleep < TTAS_MAX_BACKOFF_NANOSECONDS) backoff_sleep *= 2;

			// No value-checking, because it doesn't affect correctness:
			nanosleep(&to_sleep, NULL);

			continue;
		}

		if (!__atomic_test_and_set(&lock->lock_taken, __ATOMIC_ACQUIRE)) return;
	}
	/*
Built-in Function: bool __atomic_test_and_set (void *ptr, int memorder)
This built-in function performs an atomic test-and-set operation on the byte at *ptr. 
The byte is set to some implementation defined nonzero “set” value and the return value is true if and only if the previous contents were “set”. 
It should be only used for operands of type bool or char. For other types only part of the value may be set.

All memory orders are valid.
*/
}

void TTAS_release(struct TTAS_Lock* lock)
{
	__atomic_clear(&lock->lock_taken, __ATOMIC_RELEASE);
}
/*
Built-in Function: void __atomic_clear (bool *ptr, int memorder)
This built-in function performs an atomic clear operation on *ptr. After the operation, *ptr contains 0. 
It should be only used for operands of type bool or char and in conjunction with __atomic_test_and_set. 
For other types it may only clear partially. If the type is not bool prefer using __atomic_store.

The valid memory order variants are __ATOMIC_RELAXED, __ATOMIC_SEQ_CST, and __ATOMIC_RELEASE.
*/

//-------------
// Ticket lock 
//-------------

const unsigned TICKET_CYCLES_TO_SPIN = 100;

void TicketLock_init(struct TicketLock* lock)
{
	lock->next_ticket = 0;
	lock->now_serving = 0;
}

void TicketLock_acquire(struct TicketLock* lock)
{
	// Acquire a ticket in a queue:
	const short ticket = __atomic_fetch_add(&lock->next_ticket, 1, __ATOMIC_RELAXED);
	/*
	Built-in Function: type __atomic_fetch_add (type *ptr, type val, int memorder)

These built-in functions perform the operation suggested by the name, and return the value that had previously been in *ptr.
Operations on pointer arguments are performed as if the operands were of the uintptr_t type. 
That is, they are not scaled by the size of the type to which the pointer points.
	*/

	// On start spin-loop waiting for the lock to be released:
	for (unsigned cycle_no = 0; lock->now_serving != ticket && cycle_no < TTAS_CYCLES_TO_SPIN; ++cycle_no)
	{
		spinloop_pause();
	}

	while (__atomic_load_n(&lock->now_serving, __ATOMIC_ACQUIRE) != ticket)
	{
		sched_yield();
		/*
		 sched_yield() causes the calling thread to relinquish the CPU.
       The thread is moved to the end of the queue for its static
       priority and a new thread gets to run
       */
	}
}

void TicketLock_release(struct TicketLock* lock)
{
	__atomic_fetch_add(&lock->now_serving, 1, __ATOMIC_RELEASE);
}
