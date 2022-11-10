#include "SpinLockBenchmarks.h"
#include "SpinLocks.h"
#include <stdio.h>
#include <stdlib.h>

// TAS lock 

struct TAS_Lock TAS_test;

void TAS_test_init()
{
	
	TAS_init(&TAS_test);
}

void TAS_test_acquire()
{
	
	TAS_acquire(&TAS_test);
}

void TAS_test_release()
{
	
	TAS_release(&TAS_test);
}


// TTAS lock 

struct TTAS_Lock TTAS_test;

void TTAS_test_init()
{
	TTAS_init(&TTAS_test);
}

void TTAS_test_acquire()
{
	TTAS_acquire(&TTAS_test);
}

void TTAS_test_release()
{
	TTAS_release(&TTAS_test);
}

//--------------
// Ticket lock 
//--------------

struct TicketLock ticket_test;

void ticket_test_init()
{
	TicketLock_init(&ticket_test);
}

void ticket_test_acquire()
{
	TicketLock_acquire(&ticket_test);
}

void ticket_test_release()
{
	TicketLock_release(&ticket_test);
}


// Main 


#define NUM_LOCKS 3

const char* LOCK_NAMES[NUM_LOCKS] = 
{
	"Ticket lock",
	"TAS lock",
	"TTAS lock"
};

void (*LOCK_INITS[NUM_LOCKS])() = 
{
	ticket_test_init,
	TAS_test_init,
	TTAS_test_init
};

void (*LOCK_ACQUIRES[NUM_LOCKS])() = 
{
	ticket_test_acquire,
	TAS_test_acquire,
	TTAS_test_acquire
};

void (*LOCK_RELEASES[NUM_LOCKS])() = 
{
	ticket_test_release,
	TAS_test_release,
	TTAS_test_release
};

int main()
{
	for (unsigned lock_i = 0; lock_i < NUM_LOCKS; ++lock_i)
	{
		// Init lock:
		LOCK_INITS[lock_i]();

		// Correctness:
		printf(CYAN "%s correctness test:\n" RESET, LOCK_NAMES[lock_i]);

		run_correctness_test(LOCK_ACQUIRES[lock_i], LOCK_RELEASES[lock_i]);

		// Performance:
		printf(CYAN "%s performance test:\n" RESET, LOCK_NAMES[lock_i]);

		run_performance_test(LOCK_ACQUIRES[lock_i], LOCK_RELEASES[lock_i]);
		
		// Fairness:
		printf(CYAN "%s fairness test:\n" RESET, LOCK_NAMES[lock_i]);

		run_fairness_test(LOCK_ACQUIRES[lock_i], LOCK_RELEASES[lock_i]);
	}

	return EXIT_SUCCESS;
}
