
#ifndef SPIN_LOCKS_HPP_INCLUDED
#define SPIN_LOCKS_HPP_INCLUDED

//------------------
// Asm instructions 
//------------------

#define memory_barrier() __asm__ volatile("mfence" ::: "memory")
#define load_barrier()   __asm__ volatile("lfence" ::: "memory")
#define store_barrier()  __asm__ volatile("sfence" ::: "memory")

#define spinloop_pause() __asm__ volatile("pause")
/*
It's _mm_pause() and a compile memory barrier wrapped into one GNU C Extended ASM statement. https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html

asm("" ::: "memory") prevents compile-time reordering of memory operations across it, like C++11 std::atomic_signal_fence(std::memory_order_seq_cst). 
(not atomic_thread_fence; although on x86 preventing compile-time reordering is sufficient to make it an acquire + release fence because the only run-time reordering that x86 allows is StoreLoad.) See Jeff Preshing's Memory Ordering at Compile Time article.

Making the asm instruction part non-empty also means those asm instructions will run every time the C logically runs that source line (because it's volatile).
*/
/*

the "memory" part tells gcc that memory may have been modified inside the asm block.
While it's not true (pause doesn't affect memory content), it's used as partial anti-reorder barrier for the compiler.
Together with volatile keyword it's almost sure the pause will be at the end of the while-body 
and nothing from the code ahead will be moved after the pause, actually it may even prevent the optimizer to unroll the loop (or at least make it less likely). 
Which look as something what the author probably did want
*/

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
