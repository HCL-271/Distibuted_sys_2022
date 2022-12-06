#include "StackBenchmarks.h"
#include "Stack.h"

#include <stdlib.h>
#include <stdio.h>

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

//------
// Main 
//------

int main()
{
	// struct Stack* stack;
	// stack_init(&stack, NULL);

	// stack_push(stack, 10);
	// stack_push(stack, 20);
	// stack_push(stack, 30);
	// stack_push(stack, 40);
	// stack_push(stack, 50);

	// Data_t pop_location = 0;
	// while (stack_pop(stack, &pop_location) != -1)
	// {
	// 	printf("The value popped is %d\n", pop_location);
	// }

	// Correctness test:

	printf(CYAN "Correctness test:\n" RESET);
	run_test_correctness();

	// Efficiency tests:

	printf(CYAN "PUSH efficiency test:\n" RESET);
	run_test_efficiency_push();

	printf(CYAN "POP efficiency test:\n" RESET);
	run_test_efficiency_pop();

	printf(CYAN "PUSH-POP efficiency test:\n" RESET);
	run_test_efficiency_pop_push();
	
	// Fairness tests:

	printf(CYAN "PUSH fairness test:\n" RESET);
	run_test_fairness_push();

	printf(CYAN "POP fairness test:\n" RESET);
	run_test_fairness_pop();

	printf(CYAN "PUSH-POP fairness test:\n" RESET);
	run_test_fairness_pop_push();
	
	return EXIT_SUCCESS;
}
