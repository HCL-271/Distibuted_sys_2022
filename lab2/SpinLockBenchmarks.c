
// Multithreaded Programming
// Lab#02: Spin-lock Benchmarking


#include "SpinLockBenchmarks.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

//----------------------
// Benchmark properties 
//----------------------

const int MAX_THREADS = 100; 
const int THREAD_STEP =  10; 

const long CORRECTNESS_TEST_NUM_REPEATS          = 1;
const long CORRECTNESS_TEST_NUM_LOCK_ACQISITIONS = 100;
const long CORRECTNESS_TEST_NUMBER_OF_CYCLES     = 10;

const long PERFORMANCE_TEST_NUM_REPEATS          = 1;
const long PERFORMANCE_TEST_NUM_LOCK_ACQISITIONS = 1000;
const long PERFORMANCE_TEST_NUMBER_OF_CYCLES     = 10;

const long FAIRNESS_TEST_NUM_REPEATS          = 100;
const long FAIRNESS_TEST_NUM_LOCK_ACQISITIONS = 1;
const long FAIRNESS_TEST_NUMBER_OF_CYCLES     = 10000;

//------------------
// Common benchmark 
//------------------

struct CommonTestArgs
{
	void (*acquire_lock)();
	void (*release_lock)();

	unsigned num_lock_acuisitions;
	unsigned num_cycles_per_thread;
	unsigned num_runs;

	unsigned long number_to_increment;
};

struct TestArgs
{
	struct CommonTestArgs* common;

	pthread_t thread_id;

	double thread_execution_time;
};

void* one_thread_job(void* args)
{
	struct TestArgs*       thread_args = (struct TestArgs*) args;
	struct CommonTestArgs* common_args = (struct CommonTestArgs*) thread_args->common;

	// Measure start time:
	struct timespec start;
	if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
	{
		fprintf(stderr, MAGENTA  "[Error] Unable to get real time\n" RESET);
		exit(EXIT_FAILURE);
	}

	for (size_t acqisition = 0; acqisition < common_args->num_lock_acuisitions; ++acqisition)
	{
		common_args->acquire_lock();

		// Critical section:
		for (size_t cycle = 0; cycle < common_args->num_cycles_per_thread; ++cycle)
		{
			common_args->number_to_increment += 1;
		}

		common_args->release_lock();
	}

	// Measure finish time:
	struct timespec finish;
	if (clock_gettime(CLOCK_MONOTONIC, &finish) == -1)
	{
		fprintf(stderr, MAGENTA "[Error] Unable to get real time\n" RESET);
		exit(EXIT_FAILURE);
	}

	// Save thread execution time:
	double new_thread_execution_time = 1.00 * (finish.tv_sec  - start.tv_sec ) + 
	                                   1e-9 * (finish.tv_nsec - start.tv_nsec);

	if (thread_args->thread_execution_time < new_thread_execution_time)
		thread_args->thread_execution_time = new_thread_execution_time;

	return NULL;
}


void run_test(void (*acquire_lock)(), void (*release_lock)(),
              void (*printout_results)(struct CommonTestArgs*, struct TestArgs*, size_t),
              unsigned num_lock_acuisitions, unsigned num_cycles_per_thread, unsigned num_runs)
{
	struct CommonTestArgs common_args = 
	{
		.acquire_lock          = acquire_lock,
		.release_lock          = release_lock,
		.num_lock_acuisitions  = num_lock_acuisitions,
		.num_cycles_per_thread = num_cycles_per_thread,
		.num_runs              = num_runs,
		.number_to_increment   = 0
	};

	// Allocate memory for benchmark arguments:
	struct TestArgs* arg_array = (struct TestArgs*) malloc(MAX_THREADS * sizeof(struct TestArgs));
	if (arg_array == NULL)
	{
		fprintf(stderr, MAGENTA "[Error] Unable to get allocate memory\n" RESET);
		exit(EXIT_FAILURE);
	}

	// Fill in the argument array:
	for (size_t thread_i = 0; thread_i < MAX_THREADS; ++thread_i)
	{
		arg_array[thread_i].common = &common_args;
		arg_array[thread_i].thread_execution_time = 0.0;
	}

	// Initialize thread arguments:
	pthread_attr_t thread_attr;
	if (pthread_attr_init(&thread_attr) != 0)
	{
		fprintf(stderr, MAGENTA "[Error] Unable to init thread attributes\n" RESET);
		exit(EXIT_FAILURE);
	}

	for (size_t num_threads = THREAD_STEP; num_threads < MAX_THREADS; num_threads += THREAD_STEP)
	{
		for (size_t run = 0; run < num_runs; ++run)
		{
			// Update common_args:
			common_args.number_to_increment = 0;

			// Spawn threads:
			for (size_t thread_i = 0; thread_i < num_threads; ++thread_i)
			{
				arg_array[thread_i].thread_execution_time = 0.0;
				
				if (pthread_create(&arg_array[thread_i].thread_id, &thread_attr, one_thread_job, &arg_array[thread_i]) != 0)
				{
					fprintf(stderr, MAGENTA "[Error] Unable to create thread\n" RESET);
					exit(EXIT_FAILURE);
				}
			}

			// Join threads:
			for (size_t thread_i = 0; thread_i < num_threads; ++thread_i)
			{
				if (pthread_join(arg_array[thread_i].thread_id, NULL) != 0)
				{
					fprintf(stderr, MAGENTA "[Error] Unable to join thread\n" RESET);
					exit(EXIT_FAILURE);
				}
			}
		}

		// Printout test results:
		printout_results(&common_args, arg_array, num_threads);
	}

	free(arg_array);
}

//---------------------------
// Benchmark #1: Correctness 
//---------------------------

void correctness_test_printout(struct CommonTestArgs* common_args, struct TestArgs* arg_array, size_t num_threads)
{
	if (common_args->number_to_increment == num_threads *
			common_args->num_lock_acuisitions * common_args->num_cycles_per_thread)
	{
		printf(YELLOW "The result for %4zu threads is " GREEN "CORRECT\n" RESET, num_threads);
	}
	else
	{
		printf(YELLOW "The result for %4zu threads is " RED "WRONG\n" RESET, num_threads);
	}
}

void run_correctness_test(void (*acquire_lock)(), void (*release_lock)())
{
	run_test(acquire_lock, release_lock, correctness_test_printout, 
	         CORRECTNESS_TEST_NUM_LOCK_ACQISITIONS,
	         CORRECTNESS_TEST_NUMBER_OF_CYCLES,
	         CORRECTNESS_TEST_NUM_REPEATS);
}

//---------------------------
// Benchmark #2: Performance 
//---------------------------

void performance_test_printout(struct CommonTestArgs* common_args, struct TestArgs* arg_array, size_t num_threads)
{
	// Calculate average thread execution time:
	double average_time = 0.0;

	for (size_t thread_i = 0; thread_i < num_threads; ++thread_i)
	{
		average_time += arg_array[thread_i].thread_execution_time;
	}

	average_time /= num_threads;

	// Printout the result:
	printf(YELLOW "%4zu, %10f\n" RESET, num_threads, average_time);
}

void run_performance_test(void (*acquire_lock)(), void (*release_lock)())
{
	run_test(acquire_lock, release_lock, performance_test_printout, 
	         PERFORMANCE_TEST_NUM_LOCK_ACQISITIONS,
	         PERFORMANCE_TEST_NUMBER_OF_CYCLES,
	         PERFORMANCE_TEST_NUM_REPEATS);
}

//------------------------
// Benchmark #1: Fairness 
//------------------------

void fairness_test_printout(struct CommonTestArgs* common_args, struct TestArgs* arg_array, size_t num_threads)
{
	// Calculate maximum thread execution time:
	double max_time = 0.0;

	for (size_t thread_i = 0; thread_i < num_threads; ++thread_i)
	{
		if (max_time < arg_array[thread_i].thread_execution_time)
		{
			max_time = arg_array[thread_i].thread_execution_time;
		}
	}

	// Printout the result:
	printf(YELLOW "%4zu, %10f\n" RESET, num_threads, max_time);
}

void run_fairness_test(void (*acquire_lock)(), void (*release_lock)())
{
	run_test(acquire_lock, release_lock, fairness_test_printout, 
	         FAIRNESS_TEST_NUM_LOCK_ACQISITIONS,
	         FAIRNESS_TEST_NUMBER_OF_CYCLES,
	         FAIRNESS_TEST_NUM_REPEATS);
}
