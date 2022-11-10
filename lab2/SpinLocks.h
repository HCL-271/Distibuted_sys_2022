// No copyright. Vladislav Aleinik 2021
//======================================
// Multithreaded Programming
// Lab#02: Spin-lock Benchmarking
//======================================
#ifndef SPIN_LOCKS_HPP_INCLUDED
#define SPIN_LOCKS_HPP_INCLUDED

//------------------
// Asm instructions 
//------------------

#define memory_barrier() __asm__ volatile("mfence" ::: "memory")
#define load_barrier()   __asm__ volatile("lfence" ::: "memory")
#define store_barrier()  __asm__ volatile("sfence" ::: "memory")

#define spinloop_pause() __asm__ volatile("pause")

//------------------------------------------------------------------
// TAS lock
//------------------------------------------------------------------
// Optimizations:
// - Assembler "pause" instruction for power-effective busy-waiting
// - Exponential backoff strategy
//------------------------------------------------------------------

struct TAS_Lock
{
	volatile char lock_taken;
};

void TAS_init   (struct TAS_Lock* lock);
void TAS_acquire(struct TAS_Lock* lock);
void TAS_release(struct TAS_Lock* lock);

//------------------------------------------------------------------
// TTAS lock 
//------------------------------------------------------------------
// Optimizations:
// - Better cache-handling policy
// - Assembler "pause" instruction for power-effective busy-waiting
// - Exponential backoff strategy
//------------------------------------------------------------------

struct TTAS_Lock
{
	volatile char lock_taken;
};

void TTAS_init   (struct TTAS_Lock* lock);
void TTAS_acquire(struct TTAS_Lock* lock);
void TTAS_release(struct TTAS_Lock* lock);

//------------------------------------------------------------------
// Ticket lock 
//------------------------------------------------------------------
// Optimizations:
// - First-in first-out fairness
// - Assembler "pause" instruction for power-effective busy-waiting
// - Schedule the next thread if the lock is taken
//------------------------------------------------------------------

struct TicketLock
{
	volatile short next_ticket;
	volatile short now_serving;
};

void TicketLock_init   (struct TicketLock* lock);
void TicketLock_acquire(struct TicketLock* lock);
void TicketLock_release(struct TicketLock* lock);

#endif // SPIN_LOCKS_HPP_INCLUDED