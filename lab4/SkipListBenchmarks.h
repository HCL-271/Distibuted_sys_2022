// Benchmark #1: Correctness
// As a thread, do:
// - Insert N values
// - Search for them
// - Remove values
// - Search once more
//
// A test is passed if first two queries
// succeed and the last one does not.
//------------------------------------------------
// Benchmark #2: Insert performance
// Result is the average time to insert N values
//------------------------------------------------
// Benchmark #3: Remove performance
// Result is the average time to remove N values
//------------------------------------------------
// Benchmark #4: Search performance
// Result is the average time to search for N values
//------------------------------------------------
// Benchmark #5: Mixed performance
// As a thread, do:
// - Insert N elements
// - Search for N elements
// - Remove N elements
// Result is the average time
//------------------------------------------------
// Benchmark #6: Aggressive performance
// 90% insertions, 10% searches, 1% removals
//================================================

void run_test_correctness();
void run_test_insert_performance();
void run_test_remove_performance();
void run_test_search_performance();
void run_test_mixed_performance();
void run_test_aggressive_performance();
