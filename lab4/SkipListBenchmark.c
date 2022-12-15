#include "SkipList.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

//-------------------------
// Benchmark configuration 
//-------------------------

// Common properties:
const unsigned MAX_THREADS = 100;

// Correctness benchmark:
const int                CORRECTNESS_NUM_ELEMENTS = 100;
const int         INSERT_PERFORMANCE_NUM_ELEMENTS = 10000;
const int         REMOVE_PERFORMANCE_NUM_ELEMENTS = 10000;
const int         SEARCH_PERFORMANCE_NUM_ELEMENTS = 10000;
const int          MIXED_PERFORMANCE_NUM_ELEMENTS = 3333;
const int AGGRESIVE_PERFORMANCE_TEST_NUM_CYCLES   = 100;

//--------
// Colors 
//--------

#define RED      "\033[1;31m"
#define GREEN    "\033[1;32m"
#define YELLOW   "\033[1;33m"
#define BLUE     "\033[1;34m"
#define MAGENTA  "\033[1;35m"
#define CYAN     "\033[1;36m"
#define WHITE    "\033[0;37m"
#define RESET    "\033[0m"

//-------------------
// Helper structures 
//-------------------

struct CommonThreadArgs
{
	struct SkipList* skiplist;

	unsigned num_threads;
};

struct ThreadArgs
{
	struct CommonThreadArgs* common_args;

	pthread_t thread_id;
	unsigned  thread_i;

	bool   executed_correctly;
	double thread_execution_time;
};

//--------------
// Common stuff 
//--------------

void measure_time(struct timespec* time_ptr)
{
	if (clock_gettime(CLOCK_MONOTONIC, time_ptr) == -1)
	{
		fprintf(stderr, MAGENTA  "[Error] Unable to get real time\n" RESET);
		exit(EXIT_FAILURE);
	}
}

void nop() {}

#define START_TIME_MEASUREMENT() \
	struct timespec start;       \
	measure_time(&start)

#define FINISH_TIME_MEASUREMENT()                                                   \
	struct timespec finish;                                                         \
	measure_time(&finish);                                                          \
	                                                                                \
	thread_args->thread_execution_time = 1.00 * (finish.tv_sec  - start.tv_sec ) +  \
	                                     1e-9 * (finish.tv_nsec - start.tv_nsec)    

#define INSERT_ELEMENTS(how_many, step, shift)              \
	for (int i = 0; i < (how_many); ++i)                    \
	{                                                       \
		Key_t   key   = (step) * i + (shift);               \
		Value_t value = key;                                \
		                                                    \
		skiplist_insert(common_args->skiplist, key, value); \
	}                                                       \
	nop()


#define REMOVE_ELEMENTS(how_many, step, shift)       \
	for (int i = 0; i < (how_many); ++i)             \
	{                                                \
		Key_t key = (step) * i + (shift);            \
		                                             \
		skiplist_remove(common_args->skiplist, key); \
	}                                                \
	nop()

#define SEARCH_ELEMENTS(how_many, step, shift)               \
	for (int i = 0; i < (how_many); ++i)                     \
	{                                                        \
		Key_t   key   = (step) * i + (shift);                \
		Value_t value = 0;                                   \
		                                                     \
		skiplist_search(common_args->skiplist, key, &value); \
	}                                                        \
	nop()

void run_test(unsigned num_to_push, void* (*one_thread_job)(void* args),
              void (*printout_results)(struct CommonThreadArgs*, struct ThreadArgs*))
{
	// Allocate and fill arguments:
	struct CommonThreadArgs  common_args_val;
	struct CommonThreadArgs* common_args = &common_args_val; // Dirty hack for INSERT_ELEMENTS to work

	struct ThreadArgs* thread_args = (struct ThreadArgs*) calloc(MAX_THREADS, sizeof(struct ThreadArgs));
	if (thread_args == NULL)
	{
		fprintf(stderr, MAGENTA "[Error] Unable to allocate memory\n" RESET);
		exit(EXIT_FAILURE);
	}

	for (unsigned thread_i = 0; thread_i < MAX_THREADS; ++thread_i)
	{
		thread_args[thread_i].common_args = common_args;
		thread_args[thread_i].thread_i    = thread_i;
	}

	// Initialize thread arguments:
	pthread_attr_t thread_attr;
	if (pthread_attr_init(&thread_attr) != 0)
	{
		fprintf(stderr, MAGENTA "[Error] Unable to init thread attributes\n" RESET);
		exit(EXIT_FAILURE);
	}

	// Benchmarking process:
	for (size_t num_threads = 1; num_threads <= MAX_THREADS; num_threads += 1)
	{
		// Prepare the arguments:
		common_args->num_threads = num_threads;

		for (unsigned thr = 0; thr < num_threads; ++thr)
		{
			thread_args[thr].executed_correctly    =   1;
			thread_args[thr].thread_execution_time = 0.0;
		}

		// Init stack && pre-push values:
		skiplist_init(&common_args->skiplist);

		// Prepare the skiplist:
		INSERT_ELEMENTS(num_to_push * num_threads, 1, 0);

		// Spawn threads:
		for (unsigned thread_i = 0; thread_i < num_threads; ++thread_i)
		{
			if (pthread_create(&thread_args[thread_i].thread_id, &thread_attr, one_thread_job, &thread_args[thread_i]) != 0)
			{
				fprintf(stderr, MAGENTA "[Error] Unable to create thread\n" RESET);
				exit(EXIT_FAILURE);
			}
		}

		// Join threads:
		for (unsigned thread_i = 0; thread_i < num_threads; ++thread_i)
		{
			if (pthread_join(thread_args[thread_i].thread_id, NULL) != 0)
			{
				fprintf(stderr, MAGENTA "[Error] Unable to join thread\n" RESET);
				exit(EXIT_FAILURE);
			}
		}

		// Printout test results:
		printout_results(common_args, thread_args);

		// Free resources:
		skiplist_free(common_args->skiplist);
	}

	free(thread_args);
}

//-------------
// Correctness 
//-------------

void* correctness_thread_job(void* args)
{
	struct ThreadArgs*       thread_args = (struct ThreadArgs*) args;
	struct CommonThreadArgs* common_args = thread_args->common_args;

	START_TIME_MEASUREMENT();

	INSERT_ELEMENTS(CORRECTNESS_NUM_ELEMENTS, common_args->num_threads, thread_args->thread_i);

	// Search cycle:
	for (int i = 0; i < CORRECTNESS_NUM_ELEMENTS; ++i)
	{
		Key_t   key   = common_args->num_threads * i + thread_args->thread_i;
		Value_t value = 0;

		int retval = skiplist_search(common_args->skiplist, key, &value);

		if (retval == -1 || value != key)
		{
			printf("Not found key: %ld\n", key);
			thread_args->executed_correctly = 0;
		}
	}

	REMOVE_ELEMENTS(CORRECTNESS_NUM_ELEMENTS, common_args->num_threads, thread_args->thread_i);

	// Search cycle:
	for (int i = 0; i < CORRECTNESS_NUM_ELEMENTS; ++i)
	{
		Key_t   key   = common_args->num_threads * i + thread_args->thread_i;
		Value_t value = 0;

		int retval = skiplist_search(common_args->skiplist, key, &value);

		if (retval != -1 || value != 0)
		{
			printf("Incorrectly found key: %ld\n", key);
			thread_args->executed_correctly = 0;
		}
	}

	FINISH_TIME_MEASUREMENT();

	return NULL;
}

void correctness_test_printout(struct CommonThreadArgs* common_args, struct ThreadArgs* thread_args)
{
	for (unsigned thread_i = 0; thread_i < common_args->num_threads; ++thread_i)
	{
		if (!thread_args[thread_i].executed_correctly)
		{
			printf(YELLOW "The test for %03d threads is " RED "FAILED\n" RESET, common_args->num_threads);
			return;
		}
	}

	printf(YELLOW "The test for %03d threads is " GREEN "PASSED\n" RESET, common_args->num_threads);
}

void run_test_correctness()
{
	run_test(0, &correctness_thread_job, &correctness_test_printout);
}

//--------------------
// Insert performance 
//--------------------

void* insert_performance_thread_job(void* args)
{
	struct ThreadArgs*       thread_args = (struct ThreadArgs*) args;
	struct CommonThreadArgs* common_args = thread_args->common_args;

	START_TIME_MEASUREMENT();

	INSERT_ELEMENTS(INSERT_PERFORMANCE_NUM_ELEMENTS, common_args->num_threads, thread_args->thread_i);

	FINISH_TIME_MEASUREMENT();

	return NULL;
}

void performance_test_printout(struct CommonThreadArgs* common_args, struct ThreadArgs* thread_args)
{
	// Compute average time over several threads:
	double average = 0.0;
	for (unsigned i = 0; i < common_args->num_threads; ++i)
	{
		average += thread_args[i].thread_execution_time;
	}

	average /= common_args->num_threads;

	printf(YELLOW "%03d, " GREEN "%8f\n" RESET, common_args->num_threads, average);
}

void run_test_insert_performance()
{
	run_test(0, &insert_performance_thread_job, &performance_test_printout);
}

//--------------------
// Remove performance 
//--------------------

void* remove_performance_thread_job(void* args)
{
	struct ThreadArgs*       thread_args = (struct ThreadArgs*) args;
	struct CommonThreadArgs* common_args = thread_args->common_args;

	START_TIME_MEASUREMENT();

	REMOVE_ELEMENTS(REMOVE_PERFORMANCE_NUM_ELEMENTS, common_args->num_threads, thread_args->thread_i);

	FINISH_TIME_MEASUREMENT();

	return NULL;
}

void run_test_remove_performance()
{
	run_test(REMOVE_PERFORMANCE_NUM_ELEMENTS, &remove_performance_thread_job, &performance_test_printout);
}

//--------------------
// Search performance 
//--------------------

void* search_performance_thread_job(void* args)
{
	struct ThreadArgs*       thread_args = (struct ThreadArgs*) args;
	struct CommonThreadArgs* common_args = thread_args->common_args;

	START_TIME_MEASUREMENT();

	SEARCH_ELEMENTS(SEARCH_PERFORMANCE_NUM_ELEMENTS, common_args->num_threads, thread_args->thread_i);

	FINISH_TIME_MEASUREMENT();

	return NULL;
}

void run_test_search_performance()
{
	run_test(SEARCH_PERFORMANCE_NUM_ELEMENTS, &search_performance_thread_job, &performance_test_printout);
}

//-------------------
// Mixed performance 
//-------------------

void* mixed_performance_thread_job(void* args)
{
	struct ThreadArgs*       thread_args = (struct ThreadArgs*) args;
	struct CommonThreadArgs* common_args = thread_args->common_args;

	START_TIME_MEASUREMENT();

	INSERT_ELEMENTS(MIXED_PERFORMANCE_NUM_ELEMENTS, common_args->num_threads, thread_args->thread_i);
	SEARCH_ELEMENTS(MIXED_PERFORMANCE_NUM_ELEMENTS, common_args->num_threads, thread_args->thread_i);
	REMOVE_ELEMENTS(MIXED_PERFORMANCE_NUM_ELEMENTS, common_args->num_threads, thread_args->thread_i);

	FINISH_TIME_MEASUREMENT();

	return NULL;
}

void run_test_mixed_performance()
{
	run_test(0, &mixed_performance_thread_job, &performance_test_printout);
}

//-----------------------
// Aggresive performance 
//-----------------------

void* aggressive_performance_thread_job(void* args)
{
	struct ThreadArgs*       thread_args = (struct ThreadArgs*) args;
	struct CommonThreadArgs* common_args = thread_args->common_args;

	START_TIME_MEASUREMENT();

	for (int cycle = 0; cycle < AGGRESIVE_PERFORMANCE_TEST_NUM_CYCLES; ++cycle)
	{
		for (unsigned i = 0; i < 90; ++i)
		{
			Key_t   key   = common_args->num_threads * (AGGRESIVE_PERFORMANCE_TEST_NUM_CYCLES * cycle + thread_args->thread_i) + i;
			Value_t value = key;

			skiplist_insert(common_args->skiplist, key, value);
		}

		for (unsigned i = 0; i < 9; ++i)
		{
			Key_t   key   = common_args->num_threads * (AGGRESIVE_PERFORMANCE_TEST_NUM_CYCLES * cycle + thread_args->thread_i + 1) + i;
			Value_t value = 0;

			skiplist_search(common_args->skiplist, key, &value);
		}

		Key_t to_remove = common_args->num_threads * (AGGRESIVE_PERFORMANCE_TEST_NUM_CYCLES * cycle + thread_args->thread_i);
		skiplist_remove(common_args->skiplist, to_remove);
	}   

	FINISH_TIME_MEASUREMENT();

	return NULL;
}

void run_test_aggressive_performance()
{
	run_test(0, &aggressive_performance_thread_job, &performance_test_printout);
}
