#include "Stack.h"

#include "StackBenchmarks.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

//-------------------------
// Benchmark configuration 
//-------------------------

// Common properties:
unsigned MAX_THREADS = 64;

// Correctness test:
const unsigned CORRECTNESS_TEST_NUM_TO_PUSH = 10000;

// Efficiency test:
const unsigned EFFICIENCY_TEST_NUM_TO_PUSH = 10000;
const unsigned EFFICIENCY_TEST_NUM_TO_POP  = 10000;

// Fairness test:
const unsigned FAIRNESS_TEST_NUM_TO_PUSH = 1000;
const unsigned FAIRNESS_TEST_NUM_TO_POP  = 1000;

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
	bool check_correctness;

	unsigned num_to_push;
	unsigned num_to_pop;

	volatile char* pop_log_array;

	struct Stack* stack;
};

struct ThreadArgs
{
	struct CommonThreadArgs* common;

	pthread_t thread_id;
	unsigned  thread_i;

	double thread_execution_time;
};

//------------------
// Common benchmark 
//------------------

void* one_thread_job(void* args)
{
	struct ThreadArgs*       thread_args = (struct ThreadArgs*) args;
	struct CommonThreadArgs* common_args = thread_args->common;

	// Measure start time:
	struct timespec start;
	if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
	{
		fprintf(stderr, MAGENTA  "[Error] Unable to get real time\n" RESET);
		exit(EXIT_FAILURE);
	}

	// Push-cycle:
	for (unsigned i = 0; i < common_args->num_to_push; ++i)
	{
		unsigned to_push = common_args->num_to_push * thread_args->thread_i + i;

		stack_push(common_args->stack, to_push);
	}

	// Pop-cycle:
	if (common_args->check_correctness)
	{
		unsigned pop_location;
		for (unsigned i = 0; i < common_args->num_to_pop; ++i)
		{
			if (stack_pop(common_args->stack, &pop_location) == 0)
			{
				__atomic_add_fetch(&common_args->pop_log_array[pop_location], 1, __ATOMIC_RELAXED);
			}
		}
	}
	else
	{
		unsigned pop_location;
		for (unsigned i = 0; i < common_args->num_to_pop; ++i)
		{
			stack_pop(common_args->stack, &pop_location);
		}
	}

	// Measure finish time:
	struct timespec finish;
	if (clock_gettime(CLOCK_MONOTONIC, &finish) == -1)
	{
		fprintf(stderr, MAGENTA "[Error] Unable to get real time\n" RESET);
		exit(EXIT_FAILURE);
	}

	// Save thread execution time:
	thread_args->thread_execution_time = 1.00 * (finish.tv_sec  - start.tv_sec ) + 
	                                     1e-9 * (finish.tv_nsec - start.tv_nsec);

	return NULL;
}

void run_test(bool check_correctness, unsigned num_to_push, unsigned num_to_pop,
              void (*printout_results)(struct CommonThreadArgs*, struct ThreadArgs*, unsigned))
{
	// Allocate pop-log array:
	char* pop_log_array = NULL;
	if (check_correctness)
	{
		pop_log_array = (char*) calloc(num_to_push * MAX_THREADS, sizeof(char));
		if (pop_log_array == NULL)
		{
			fprintf(stderr, MAGENTA "[Error] Unable to allocate memory\n" RESET);
			exit(EXIT_FAILURE);
		}
	}

	// Allocate and fill arguments:
	struct CommonThreadArgs common_args =
	{
		.check_correctness = check_correctness,
		.num_to_push       = num_to_push,
		.num_to_pop        = num_to_pop,
		.pop_log_array     = pop_log_array
	};

	struct ThreadArgs* thread_args = (struct ThreadArgs*) calloc(MAX_THREADS, sizeof(struct ThreadArgs));
	if (thread_args == NULL)
	{
		fprintf(stderr, MAGENTA "[Error] Unable to allocate memory\n" RESET);
		exit(EXIT_FAILURE);
	}

	for (unsigned thread_i = 0; thread_i < MAX_THREADS; ++thread_i)
	{
		thread_args[thread_i].common                = &common_args;
		thread_args[thread_i].thread_i              = thread_i;
		thread_args[thread_i].thread_execution_time = 0.0;
	}

	// Initialize thread arguments:
	pthread_attr_t thread_attr;
	if (pthread_attr_init(&thread_attr) != 0)
	{
		fprintf(stderr, MAGENTA "[Error] Unable to init thread attributes\n" RESET);
		exit(EXIT_FAILURE);
	}

	// Spawn threads:
	for (size_t num_threads = 4; num_threads <= MAX_THREADS; num_threads += 4)
	{
		if (check_correctness)
		{
			memset(pop_log_array, 0, num_to_push * num_threads);
		}

		// Init stack && pre-push values:
		stack_init(&common_args.stack, NULL);

		for (unsigned to_push = 0; to_push + num_to_push < num_to_pop; ++to_push)
		{
			stack_push(common_args.stack, to_push);
		}

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
		printout_results(&common_args, thread_args, num_threads);

		// Free stack:
		stack_free(common_args.stack);
	}

	// Deallocate resources:
	if (check_correctness) free(pop_log_array);

	free(thread_args);
}

//---------------------------
// Benchmark #1: Correctness 
//---------------------------

void correctness_test_printout(struct CommonThreadArgs* common_args, struct ThreadArgs* arg_array, unsigned num_threads)
{
	// Check if the stack is empty:
	unsigned pop_location;
	if (stack_pop(common_args->stack, &pop_location) != -1)
	{
		printf(YELLOW "The test for %4d threads is " RED "FAILED\n" RESET, num_threads);
		return;
	}

	// Check pop-log array:
	for (unsigned i = 0; i < num_threads * common_args->num_to_push; ++i)
	{
		if (common_args->pop_log_array[i] != 1)
		{
			printf(YELLOW "The test for %4d threads is " RED "FAILED\n" RESET, num_threads);
			return;
		}
	}

	printf(YELLOW "The test for %4d threads is " GREEN "PASSED\n" RESET, num_threads);

}

void run_test_correctness()
{
	run_test(1, CORRECTNESS_TEST_NUM_TO_PUSH, CORRECTNESS_TEST_NUM_TO_PUSH, &correctness_test_printout);
}

//-----------------------------
// Benchmarks #2-4: Efficiency 
//-----------------------------

void efficiency_test_printout(struct CommonThreadArgs* common_args, struct ThreadArgs* arg_array, unsigned num_threads)
{
	// Compute average time over several threads:
	double average = 0.0;
	for (unsigned i = 0; i < num_threads; ++i)
	{
		average += arg_array[i].thread_execution_time;
	}

	average /= num_threads;

	printf(YELLOW "%4d, " GREEN "%10f\n" RESET, num_threads, average);
}

void run_test_efficiency_push()
{
	run_test(0, EFFICIENCY_TEST_NUM_TO_PUSH, 0, &efficiency_test_printout);
}

void run_test_efficiency_pop()
{
	run_test(0, 0, EFFICIENCY_TEST_NUM_TO_POP, &efficiency_test_printout);
}

void run_test_efficiency_pop_push()
{
	run_test(0, EFFICIENCY_TEST_NUM_TO_PUSH, EFFICIENCY_TEST_NUM_TO_POP, &efficiency_test_printout);
}

//---------------------------
// Benchmarks #5-7: Fairness 
//---------------------------

void fairness_test_printout(struct CommonThreadArgs* common_args, struct ThreadArgs* arg_array, unsigned num_threads)
{
	// Compute maximum time over several threads:
	double maximum = 0.0;
	for (unsigned i = 0; i < num_threads; ++i)
	{
		double cur = arg_array[i].thread_execution_time;
		if (cur > maximum)
		{
			maximum = cur;
		}
	}

	printf(YELLOW "%4u, " GREEN "%10f\n" RESET, num_threads, maximum);
}

void run_test_fairness_push()
{
	run_test(0, FAIRNESS_TEST_NUM_TO_PUSH, 0, &fairness_test_printout);
}

void run_test_fairness_pop()
{
	run_test(0, 0, FAIRNESS_TEST_NUM_TO_POP, &fairness_test_printout);
}

void run_test_fairness_pop_push()
{
	run_test(0, FAIRNESS_TEST_NUM_TO_PUSH, FAIRNESS_TEST_NUM_TO_POP, &fairness_test_printout);
}
