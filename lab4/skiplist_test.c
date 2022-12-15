#include "SkipList.h"

#include "SkipListBenchmarks.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
	printf(CYAN "Correctness test:\n" RESET);
	run_test_correctness();

	printf(CYAN "Insert performance test:\n" RESET);
	run_test_insert_performance();

	printf(CYAN "Search performance test:\n" RESET);
	run_test_search_performance();

	printf(CYAN "Remove performance test:\n" RESET);
	run_test_remove_performance();

	printf(CYAN "Mixed performance test:\n" RESET);
	run_test_mixed_performance();

	printf(CYAN "Aggressive performance test:\n" RESET);
	run_test_aggressive_performance();

	return EXIT_SUCCESS;
}
