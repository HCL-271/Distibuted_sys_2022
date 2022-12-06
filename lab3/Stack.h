typedef unsigned Data_t;
typedef char bool;

struct Stack;

//----------------------
// Stack initialization
//----------------------

int  stack_init(struct Stack** stack_ptr, void (*data_destructor)(Data_t*));
void stack_free(struct Stack* stack);

//----------------------
// Stack element access
//----------------------

// Push the value into the stack:
int stack_push(struct Stack* stack, Data_t data);

// Pop the value from the stack:
int stack_pop(struct Stack* stack, Data_t* place_to_pop);

//----------------------
// Stack error-checking
//----------------------

// Stack error numbers:
enum StackErrno
{
	NO_ERROR           = 0,
	NO_MEMORY          = 1,
	NO_ELEMENT         = 2,
	NO_THREADS         = 3,
	INVALID_ARGUMENT   = 4,
	THREAD_LOCAL_ERROR = 5
};

// Get stack error number:
int stack_get_errno(struct Stack* stack);

// Print stack error message:
void stack_print_errno(struct Stack* stack);
