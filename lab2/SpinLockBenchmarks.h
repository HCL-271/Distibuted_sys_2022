// Beerware license. Vladislav Aleinik 2021
//===================================================================
// Multithreaded Programming
// Lab#02: Spin-lock Benchmarking
//===================================================================
// Benchmark #1: Correctness
// P threads collectively increment a variable N times in a critical
// section governed by spinlock. Test shows, whether the resulting
// sum is equal to the theoretical value P*N.
//-------------------------------------------------------------------
// Benchmark #2: Perfomance
// P threads perform a handful of lock acuisitions for an average
// time Ta. Plot Ta(P) is the output.
//-------------------------------------------------------------------
// Benchmark #3: Fairness
// P threads perform a handful of lock acuisitions for some job.
// Each lock acquisition time is measured. Maximum lock acquisition
// time Tm is computed. Plot Tm(P) is the output.
//-------------------------------------------------------------------
#ifndef SPIN_LOCK_BENCHMARKS_HPP_INCLUDED
#define SPIN_LOCK_BENCHMARKS_HPP_INCLUDED

//---------------
// Miscellaneous 
//---------------

// Bring color to one's life:
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA  "\033[1;35m"
#define CYAN    "\033[1;36m"
#define WHITE   "\033[0;37m"
#define RESET   "\033[0m"

//---------------------------
// Benchmark #1: Correctness 
//---------------------------

void run_correctness_test(void (*acquire_lock)(), void (*release_lock)());

//---------------------------
// Benchmark #2: Performance 
//---------------------------

void run_performance_test(void (*acquire_lock)(), void (*release_lock)());

//------------------------
// Benchmark #1: Fairness 
//------------------------

void run_fairness_test(void (*acquire_lock)(), void (*release_lock)());

#endif // SPIN_LOCK_BENCHMARKS_HPP_INCLUDED