// Benchmark #1: Correctness
// Each thread first pushes N values, then pops N values
// Values are distinct for all threads
// The test is passed if every value pushed was popped and the stack is empty
//------------------------------------------------
// Benchmark #2: Push Efficiency
// Each thread pushes N values.
// Average time is the result.
//------------------------------------------------
// Benchmark #3: Pop Efficiency
// A stack is prefilled with N*P values
// Each thread pops N values.
// Average time is the result.
//------------------------------------------------
// Benchmark #4: Push+Pop Efficiency
// Each thread first pushes N values, then pops N values
// Average time is the result.
//------------------------------------------------
// Benchmark #5: Push Fairness
// Each thread pushes N values.
// Maximum time is the result.
//------------------------------------------------
// Benchmark #6: Pop Fairness
// A stack is prefilled with N*P values
// Each thread pops N values.
// Maximum time is the result.
//------------------------------------------------
// Benchmark #7: Push+Pop Fairness
// Each thread first pushes N values, then pops N values
// Maximum time is the result.
//------------------------------------------------

void run_test_correctness();
void run_test_efficiency_push();
void run_test_efficiency_pop();
void run_test_efficiency_pop_push();
void run_test_fairness_push();
void run_test_fairness_pop();
void run_test_fairness_pop_push();
