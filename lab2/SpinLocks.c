
// Multithreaded Programming
// Lab2: Spin-lock Benchmarking


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

//-----------
// TTAS lock 
//-----------

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
}

void TTAS_release(struct TTAS_Lock* lock)
{
	__atomic_clear(&lock->lock_taken, __ATOMIC_RELEASE);
}

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

	// On start spin-loop waiting for the lock to be released:
	for (unsigned cycle_no = 0; lock->now_serving != ticket && cycle_no < TTAS_CYCLES_TO_SPIN; ++cycle_no)
	{
		spinloop_pause();
	}

	while (__atomic_load_n(&lock->now_serving, __ATOMIC_ACQUIRE) != ticket)
	{
		sched_yield();
	}
}

void TicketLock_release(struct TicketLock* lock)
{
	__atomic_fetch_add(&lock->now_serving, 1, __ATOMIC_RELEASE);
}
