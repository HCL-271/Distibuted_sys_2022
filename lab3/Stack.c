#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <memory.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "Stack.h"

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

//------------------
// Stack structures 
//------------------

// Stack list node:
struct StackNode
{
	volatile struct StackNode* next;

	Data_t data;
};

// Hazard pointer:
struct HazardPointer
{
	volatile pid_t id;
	volatile struct StackNode* pointer;
};

const unsigned MAX_HAZARD_POINTERS = 64;

struct Stack
{
	// Hazard pointer thread-local storage:
	pthread_key_t thread_local_key;

	// Hazard pointer array:
	volatile struct HazardPointer* hazard_pointers;

	// Reclamation list && user data destructor:
	volatile struct StackNode* to_reclaim;
	void (*data_destructor)(Data_t*);
	volatile unsigned reclaim_counter;

	// Stack operation:
	volatile struct StackNode* head;

	// Stack error-checking:
	volatile int errno;
};

//-------------------------------
// Hazard pointer implementation 
//-------------------------------

int hazard_pointer_init(struct Stack* stack, struct HazardPointer** hp_ptr)
{
	if (*hp_ptr != NULL)
	{
		return 0;
	}

	// Get current thread id:
	pid_t thread_id = syscall(SYS_gettid);

	*hp_ptr = NULL;

	for (unsigned hp_i = 0; hp_i < MAX_HAZARD_POINTERS; ++hp_i)
	{
		pid_t old_id = 0;

		// Acquire a hazard pointer with a strong CAS, if it isn't taken already:
		if (__atomic_compare_exchange_n(&stack->hazard_pointers[hp_i].id, &old_id, &thread_id, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED))
		{
			*hp_ptr = (struct HazardPointer*) &stack->hazard_pointers[hp_i];

			break;
		}
	}

	if (*hp_ptr == NULL)
	{
		stack->errno = NO_THREADS;
		return -1; 
	}

	return 0;
}

void hazard_pointer_cleanup(void* arg)
{
	volatile struct HazardPointer* hp_ptr = (struct HazardPointer*) arg;

	// Clear the hazard pointer:
	__atomic_store_n(&hp_ptr->pointer, NULL, __ATOMIC_RELAXED);
	__atomic_store_n(&hp_ptr->id,         0, __ATOMIC_RELAXED);
}

struct HazardPointer* get_hazard_pointer_for_current_thread(struct Stack* stack)
{
	struct HazardPointer* hp = (struct HazardPointer*) pthread_getspecific(stack->thread_local_key);

	if (hp == NULL)
	{
		hazard_pointer_init(stack, &hp);
	}

	if (pthread_setspecific(stack->thread_local_key, hp))
	{
		stack->errno = THREAD_LOCAL_ERROR;
		return NULL;
	}

	return hp;
}

bool outstanding_hazard_pointers_for(struct Stack* stack, struct StackNode* danger)
{
	for (unsigned hp_i = 0; hp_i < MAX_HAZARD_POINTERS; ++hp_i)
	{
		if (__atomic_load_n(&stack->hazard_pointers[hp_i].pointer, __ATOMIC_RELAXED) == danger)
		{
			return 1;
		}
	}

	return 0;
}

//--------------------
// Memory reclamation 
//--------------------

void add_to_reclaim_list(struct Stack* stack, struct StackNode* node)
{
	volatile struct StackNode* reclaim_head;

	// Spin until the update of the reclaim list head is successful:
	do
	{
		reclaim_head = (struct StackNode*) __atomic_load_n(&stack->to_reclaim, __ATOMIC_RELAXED);
		
		__atomic_store_n(&node->next, reclaim_head, __ATOMIC_RELAXED);
	}
	while (!__atomic_compare_exchange_n(&stack->to_reclaim, &node->next, node, 1, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
}

void delete_nodes_with_no_hazards(struct Stack* stack)
{
	// Acquire exclusive access to the reclaim list:
	struct StackNode* current = (struct StackNode*) __atomic_exchange_n(&stack->to_reclaim, NULL, __ATOMIC_RELAXED);

	while (current != NULL)
	{
		struct StackNode* next = (struct StackNode*) current->next;

		if (!outstanding_hazard_pointers_for(stack, current))
		{
			// Delete non-hazard node:
			free(current);
		}
		else
		{
			// Return it to reclaim list otherwise:
			add_to_reclaim_list(stack, current);
		}

		current = next;
	}
}

//---------------------------------
// Stack constructor && destructor 
//---------------------------------

int stack_init(struct Stack** stack_ptr, void (*data_destructor)(Data_t*))
{
	// Check arguments:
	if (stack_ptr == NULL)
	{
		return -1;
	}

	// Allocate memory for the stack:
	*stack_ptr = malloc(sizeof(struct Stack));
	if (*stack_ptr == NULL)
	{
		return -1;
	}

	struct Stack* stack = *stack_ptr;

	// Thread local storage for hazard pointers:
	if (pthread_key_create(&stack->thread_local_key, &hazard_pointer_cleanup) != 0)
	{
		stack->errno = THREAD_LOCAL_ERROR;
		return -1;
	} 

	// Create hazard pointer array:
	stack->hazard_pointers = calloc(MAX_HAZARD_POINTERS, sizeof(struct HazardPointer));
	if (stack->hazard_pointers == NULL)
	{
		stack->errno = NO_MEMORY;
		return -1;
	}

	// Reclamation:
	stack->to_reclaim = NULL;
	stack->data_destructor = data_destructor;
	stack->reclaim_counter = 0;

	// Error checking:
	stack->head  = NULL;
	stack->errno = NO_ERROR;

	return 0;
}

void stack_free(struct Stack* stack)
{
	// Check arguments:
	if (stack == NULL) return;

	// Assumption: calling thread has exclusive access to the stack 

	// Delete stack nodes:
	struct StackNode* to_delete = (struct StackNode*) stack->head;
	while (to_delete != NULL)
	{
		struct StackNode* next = (struct StackNode*) to_delete->next;

		if (stack->data_destructor != NULL)
		{
			stack->data_destructor(&to_delete->data);
		}

		free(to_delete);

		to_delete = next;
	}

	// Reclaim deleted nodes:
	to_delete = (struct StackNode*) stack->to_reclaim;
	while (to_delete != NULL)
	{
		struct StackNode* next = (struct StackNode*) to_delete->next;

		free(to_delete);

		to_delete = next;
	}

	free((struct StackNode*) stack->hazard_pointers);

	// Delete thread-local storage key:
	pthread_key_delete(stack->thread_local_key);

	// Free stack memory:
	free(stack);
}

//-----------------
// Stack insertion 
//-----------------

const unsigned YIELD_BACKOFF = 2;

int stack_push(struct Stack* stack, Data_t data)
{
	// Check arguments:
	if (stack == NULL)
	{
		return -1;
	}

	// Allocate node to store in the stack:
	struct StackNode* new_node = malloc(sizeof(struct StackNode));
	if (new_node == NULL)
	{
		stack->errno = NO_MEMORY;
		return -1;
	}

	// Put data into the node:
	__atomic_store_n(&new_node->data, data, __ATOMIC_RELAXED);

	struct StackNode* top;

	unsigned backoff_counter = 0;
	do
	{
		if (backoff_counter == YIELD_BACKOFF)
		{
			backoff_counter = 0;
			sched_yield();
		}

		// Acquire the top of the stack:
		top = (struct StackNode*) __atomic_load_n(&stack->head, __ATOMIC_RELAXED);

		// Current node is the new head:
		__atomic_store_n(&new_node->next, top, __ATOMIC_RELAXED);

		backoff_counter += 1;

	}
	// Keep spinning until the head is successfully set:
	while (!__atomic_compare_exchange_n(&stack->head, &top, new_node, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

	return 0;
}

//----------------
// Stack deletion 
//----------------

int stack_pop(struct Stack* stack, Data_t* place_to_pop)
{
	// Check arguments:
	if (stack == NULL)
	{
		return -1;
	}

	if (place_to_pop == NULL)
	{
		stack->errno = INVALID_ARGUMENT;
		return -1;
	}

	// Get hazard pointer:
	struct HazardPointer* hp = get_hazard_pointer_for_current_thread(stack);

	struct StackNode* old_head;
	do
	{
		old_head = (struct StackNode*) __atomic_load_n(&stack->head, __ATOMIC_ACQUIRE);
		struct StackNode* tmp;

		// Loop until hazard pointer is pointing to the node being deleted:
		do
		{
			tmp = old_head;
			
			__atomic_store_n(&hp->pointer, old_head, __ATOMIC_RELAXED);

			old_head = (struct StackNode*) __atomic_load_n(&stack->head, __ATOMIC_RELAXED);
		}
		while (old_head != tmp);
	}
	while (old_head != NULL &&
	       !__atomic_compare_exchange_n(&stack->head, &old_head, old_head->next, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));

	// Clear the hazard pointer:
	__atomic_store_n(&hp->pointer, NULL, __ATOMIC_RELAXED);

	// Return an error if stack is empty:
	if (old_head == NULL)
	{
		stack->errno = NO_ELEMENT;
		return -1;
	}

	// Copy the element out of the stack:
	*place_to_pop = old_head->data;

	// 	Add the deleted node to reclaim list:
	add_to_reclaim_list(stack, old_head);

	unsigned reclaim_counter = __atomic_add_fetch(&stack->reclaim_counter, 1, __ATOMIC_RELAXED);

	// Delete non-hazardous nodes:
	if (reclaim_counter >= 2 * MAX_HAZARD_POINTERS)
	{
		__atomic_store_n(&stack->reclaim_counter, 0, __ATOMIC_RELAXED);

		delete_nodes_with_no_hazards(stack);
	}

	return 0;
}

//----------------------
// Stack error-checking 
//----------------------

int stack_get_errno(struct Stack* stack)
{
	return stack->errno;
}

void stack_print_error(struct Stack* stack)
{
	switch (stack->errno)
	{
		case NO_ERROR:
			return;
		case NO_MEMORY:
			fprintf(stderr, MAGENTA"[Error] Unable to allocate memory\n"RESET);
			return;
		case NO_ELEMENT:
			fprintf(stderr, MAGENTA"[Error] Unable to pop from empty stack\n"RESET);
			return;
		case NO_THREADS:
			fprintf(stderr, MAGENTA"[Error] Stack can't handle more threads\n"RESET);
			return;
		default:
			fprintf(stderr, MAGENTA"[Error] Unknown error number\n"RESET);
	}
}
