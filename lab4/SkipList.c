#include "SkipList.h"

#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

//-----------
// Constants 
//-----------

// Auxulary constants:
const int KEY_MIN = INT_MIN;

const bool FALSE = 0;
const bool TRUE  = 1;

const bool WEAK   = 1;
const bool STRONG = 0; 

// Performance constants:
#define MAX_THREADS 256
#define NUM_LEVELS   16
#define NUM_EPOCHS    4

//-----------------
// Tagged pointers
//-----------------

#define    MARKED(ptr) ((struct Tower*) (((intptr_t) ptr) |  0x01))
#define   POINTER(ptr) ((struct Tower*) (((intptr_t) ptr) & ~0x01))
#define IS_MARKED(ptr) ((bool)          (((intptr_t) ptr) &  0x01))

//---------------------
// Skiplist structures 
//---------------------

struct Tower
{
	Key_t   key;
	Value_t value;

	volatile unsigned level;

	volatile struct Tower* reclaim_next;

	volatile struct Tower* forward[];
};

struct SkipList
{
	// Thread-local storage:
	pthread_key_t thread_local_key;

	// Skiplist search optimization:
	volatile unsigned max_level;

	// Error-checking:
	int errno;

	// Reclamation management:
	volatile struct Tower* reclaim_lists[NUM_EPOCHS];

	volatile bool reclaim_lock;
	volatile char global_epoch;
	volatile char local_epochs[MAX_THREADS];

	// Data:
	struct Tower header;
};

//-------------------------
// SkipList initialization 
//-------------------------

int skiplist_init(struct SkipList** skiplist_ptr)
{
	if (skiplist_ptr == NULL) return -1;

	// Allocate memory for the skiplist:
	*skiplist_ptr = (struct SkipList*) malloc(sizeof(struct SkipList) + sizeof(struct Tower*) * NUM_LEVELS);
	if (*skiplist_ptr == NULL) return -1;

	struct SkipList* skiplist = *skiplist_ptr;

	// Thread local storage for local epochs:
	if (pthread_key_create(&skiplist->thread_local_key, NULL) != 0)
	{
		skiplist->errno = THREAD_LOCAL_ERROR;
		return -1;
	}

	skiplist->max_level = 0;
	skiplist->errno     = NO_ERROR;

	// Initialize reclamation parameters: 
	for (unsigned epoch = 0; epoch < NUM_EPOCHS; ++epoch)
	{
		skiplist->reclaim_lists[epoch] = NULL;
		skiplist->local_epochs[epoch]  = -1;
	}

	skiplist->reclaim_lock = 0;
	skiplist->global_epoch = 0;

	// Initialize header tower:
	skiplist->header.key          = KEY_MIN;
	skiplist->header.level        = NUM_LEVELS - 1;
	skiplist->header.reclaim_next = NULL;

	for (unsigned lvl = 0; lvl < NUM_LEVELS; ++lvl)
	{
		skiplist->header.forward[lvl] = NULL;
	}

	return 0;
}

int skiplist_free(struct SkipList* skiplist)
{
	if (skiplist == NULL) return -1;

	// Free all non-removed towers:
	struct Tower* to_delete = POINTER(skiplist->header.forward[0]);

	while (to_delete != NULL)
	{
		struct Tower* next = POINTER(to_delete->forward[0]);

		free(to_delete);

		to_delete = next;
	}

	// Free all removed towers:
	for (unsigned epoch = 0; epoch < NUM_EPOCHS; ++epoch)
	{
		to_delete = (struct Tower*) skiplist->reclaim_lists[epoch];

		while (to_delete != NULL)
		{
			struct Tower* next = (struct Tower*) to_delete->reclaim_next;

			free(to_delete);

			to_delete = next;
		}
	}

	// Delete thread-local storage key:
	pthread_key_delete(skiplist->thread_local_key);

	free(skiplist);

	return 0;
}

//--------------------
// Auxulary functions 
//--------------------

unsigned generate_level()
{
	unsigned generated = 0;

	int rand = random();
	while (generated < NUM_LEVELS - 1 && rand < RAND_MAX/2)
	{
		generated += 1;

		rand = random();
	}

	return generated;
}


void fill_tables(struct SkipList* skiplist, struct Tower** preds, struct Tower** succs, Key_t search_key,
                 int from_level, int to_level)
{
	struct Tower* cur;

	reset_search:
	cur = &(skiplist->header); 

	// Go through all skiplist levels from top to bottom:
	for (int lvl = from_level; to_level <= lvl; --lvl)
	{
		volatile struct Tower* next = POINTER(__atomic_load_n(&cur->forward[lvl], __ATOMIC_ACQUIRE));

		while (next != NULL)
		{
			struct Tower* next_next = (struct Tower*) __atomic_load_n(&next->forward[lvl], __ATOMIC_RELAXED);
			
			bool marked = IS_MARKED(next_next);

			// Repeatedly remove next node if it is marked:
			if (marked)
			{
				if (!__atomic_compare_exchange_n(&cur->forward[lvl], &next, POINTER(next_next), STRONG, __ATOMIC_RELEASE, __ATOMIC_RELAXED))
				{
					goto reset_search;
				}

				next = POINTER(__atomic_load_n(&cur->forward[lvl], __ATOMIC_ACQUIRE));
			}
			// Move to the next key:
			else if (__atomic_load_n(&next->key, __ATOMIC_RELAXED) < search_key)
			{
				cur  = POINTER(next);
				next = POINTER(next_next);
			}
			else break;
		}

		preds[lvl] = cur;
		succs[lvl] = POINTER(next);
	}
}

//------------------------
// Reclamation management 
//------------------------

char* epoch_init(struct SkipList* skiplist)
{
	char global_epoch = __atomic_load_n(&skiplist->global_epoch, __ATOMIC_RELAXED);

	char previous = -1;

	// Allocate the cell in the local epoch list:
	for (unsigned cell = 0; cell < MAX_THREADS; ++cell)
	{
		if (__atomic_compare_exchange_n(&skiplist->local_epochs[cell], &previous, global_epoch, STRONG, __ATOMIC_RELEASE, __ATOMIC_RELAXED))
		{
			return (char*) &skiplist->local_epochs[cell];
		}
	}

	skiplist->errno = NO_THREADS;
	return NULL;
}

char enter_epoch(struct SkipList* skiplist)
{	
	char* local_epoch = (char*) pthread_getspecific(skiplist->thread_local_key);

	if (local_epoch == NULL)
	{
		local_epoch = epoch_init(skiplist);

		if (local_epoch == NULL) return -1;

		if (pthread_setspecific(skiplist->thread_local_key, local_epoch) == -1)
		{
			skiplist->errno = THREAD_LOCAL_ERROR;
			return -1;
		}
	}

	return *local_epoch;
}

void leave_epoch(struct SkipList* skiplist, char epoch)
{
	char* local_epoch = (char*) pthread_getspecific(skiplist->thread_local_key);

	char global_epoch = __atomic_load_n(&skiplist->global_epoch, __ATOMIC_RELAXED);

	if (*local_epoch != global_epoch)
	{
		__atomic_store_n(local_epoch, (*local_epoch + 1) % NUM_EPOCHS, __ATOMIC_RELAXED);
	}
}

void add_to_reclaim_list(struct SkipList* skiplist, struct Tower* to_reclaim, unsigned epoch)
{
	struct Tower* reclaim_head = NULL;

	// Spin until the update of the reclaim list head is successful:
	do
	{
		reclaim_head = (struct Tower*) __atomic_load_n(&skiplist->reclaim_lists[epoch], __ATOMIC_ACQUIRE);

		__atomic_store_n(&to_reclaim->reclaim_next, reclaim_head, __ATOMIC_RELAXED);
	}
	while (!__atomic_compare_exchange_n(&skiplist->reclaim_lists[epoch], &reclaim_head, to_reclaim,
	                                    STRONG, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
}

void reclaim_nodes(struct SkipList* skiplist)
{
	// Try to take the reclaim lock:
	if (__atomic_test_and_set(&skiplist->reclaim_lock, __ATOMIC_ACQUIRE))
	{
		// Don't do reclamation if somebody is already doing it:
		return;
	}

	// Move the current epoch if possible:
	char global_epoch = __atomic_load_n(&skiplist->global_epoch, __ATOMIC_RELAXED);

	bool change_epoch = TRUE;
	for (unsigned thr_i = 0; thr_i < MAX_THREADS; ++thr_i)
	{
		char epoch = __atomic_load_n(&skiplist->local_epochs[thr_i], __ATOMIC_RELAXED);

		if (epoch != global_epoch)
		{
			change_epoch = FALSE;
			break;
		}
	}

	// Perform reclamation:
	if (change_epoch)
	{
		unsigned reclaim_epoch = (global_epoch + NUM_EPOCHS - 2) % NUM_EPOCHS;

		struct Tower* reclaim_head = (struct Tower*) __atomic_exchange_n(&skiplist->reclaim_lists[reclaim_epoch], NULL, __ATOMIC_RELAXED);

		while (reclaim_head != NULL)
		{
			struct Tower* to_be_freed = reclaim_head;

			reclaim_head = (struct Tower*) __atomic_load_n(&reclaim_head->reclaim_next, __ATOMIC_RELAXED);

			free(to_be_freed);
		}

		unsigned epoch_next = (global_epoch + 1) % NUM_EPOCHS;

		__atomic_store_n(&skiplist->global_epoch, epoch_next, __ATOMIC_RELAXED);
	}

	// Free the reclamation lock:
	__atomic_clear(&skiplist->reclaim_lock, __ATOMIC_RELEASE);
}

//--------------------
// SkipList insertion 
//--------------------

int skiplist_insert(struct SkipList* skiplist, Key_t search_key, Value_t value)
{
	if (skiplist == NULL) return -1;

	struct Tower* preds[NUM_LEVELS];
	struct Tower* succs[NUM_LEVELS];

	// Enter epoch:
	unsigned epoch = enter_epoch(skiplist);

	// Search for the insertion place:
	fill_tables(skiplist, preds, succs, search_key, NUM_LEVELS - 1, 0);

	// Check the value to the right of the selected tower:
	struct Tower* cur = succs[0];

	// Update the value if the key is already in the skiplist:
	if (cur != NULL && __atomic_load_n(&cur->key, __ATOMIC_RELAXED) == search_key)
	{
		__atomic_store_n(&cur->value, value, __ATOMIC_RELAXED);

		// Leave epoch:
		leave_epoch(skiplist, epoch);
		return 0;
	}

	// Insert the node into the skiplist otherwise:
	else
	{
		// Generate level of the new tower node:
		unsigned new_level = generate_level();

		// Allocate memory for the new tower:
		struct Tower* new_tower = (struct Tower*) malloc(sizeof(struct Tower) + (new_level + 1) * sizeof(struct Tower*));
		if (new_tower == NULL)
		{
			skiplist->errno = NO_MEMORY;
			return -1;
		}

		// Fill in tower attributes:
		__atomic_store_n(&new_tower->key         , search_key, __ATOMIC_RELAXED);
		__atomic_store_n(&new_tower->value       ,      value, __ATOMIC_RELAXED);
		__atomic_store_n(&new_tower->level       ,  new_level, __ATOMIC_RELAXED);
		__atomic_store_n(&new_tower->reclaim_next,       NULL, __ATOMIC_RELAXED);

		// Update max level:
		if (new_level > __atomic_load_n(&skiplist->max_level, __ATOMIC_RELAXED))
		{
			__atomic_store_n(&skiplist->max_level, new_level, __ATOMIC_RELAXED);
		}

		// Insert the tower level by level from bottom to top:
		for (unsigned lvl = 0; lvl <= new_level; ++lvl)
		{
			__atomic_store_n(&new_tower->forward[lvl], succs[lvl], __ATOMIC_RELAXED);

			while (!__atomic_compare_exchange_n(&preds[lvl]->forward[lvl], &succs[lvl], new_tower,
			                                    STRONG, __ATOMIC_RELEASE, __ATOMIC_RELAXED))
			{
				// Refill the tables if tower was marked or changed in the process:
				fill_tables(skiplist, preds, succs, search_key, NUM_LEVELS - 1, lvl);

				__atomic_store_n(&new_tower->forward[lvl], succs[lvl], __ATOMIC_RELAXED);
			}
		}
	}

	// Leave epoch:
	leave_epoch(skiplist, epoch);

	return 0;
}

//-------------------
// SkipList deletion 
//-------------------

int skiplist_remove(struct SkipList* skiplist, Key_t search_key)
{
	if (skiplist == NULL) return -1;

	struct Tower* preds[NUM_LEVELS];
	struct Tower* succs[NUM_LEVELS];

	// Enter epoch:
	unsigned epoch = enter_epoch(skiplist);

	// Search for the key:
	fill_tables(skiplist, preds, succs, search_key, NUM_LEVELS - 1, 0);

	volatile struct Tower* cur = succs[0];

	if (cur == NULL || __atomic_load_n(&cur->key, __ATOMIC_RELAXED) != search_key)
	{
		// Leave epoch:
		leave_epoch(skiplist, epoch);

		return 0;
	}

	// Mark elements from top to bottom:
	unsigned max_level = __atomic_load_n(&cur->level, __ATOMIC_RELAXED);
	for (unsigned lvl = max_level; 0 < lvl; --lvl)
	{
		volatile struct Tower* next;

		do
		{
			next = __atomic_load_n(&cur->forward[lvl], __ATOMIC_ACQUIRE);
		}
		while (!__atomic_compare_exchange_n(&cur->forward[lvl], &next, MARKED(next), STRONG, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
	}

	// Mark the bottom node:
	bool i_marked_it = FALSE;
	volatile struct Tower* next;
	do
	{
		next = POINTER(__atomic_load_n(&cur->forward[0], __ATOMIC_ACQUIRE));

		i_marked_it = __atomic_compare_exchange_n(&cur->forward[0], &next, MARKED(next), STRONG, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
	}
	while (!IS_MARKED(next) && !i_marked_it);

	if (i_marked_it)
	{
		// Unlink tower by searching for it:
		fill_tables(skiplist, preds, succs, search_key, NUM_LEVELS - 1, 0);

		// Add tower to reclaim list:
		add_to_reclaim_list(skiplist, (struct Tower*) cur, epoch);
	}

	// Leave epoch:
	leave_epoch(skiplist, epoch);

	if (i_marked_it) reclaim_nodes(skiplist);

	return 0;
}

//-----------------
// SkipList search 
//-----------------

int skiplist_search(struct SkipList* skiplist, Key_t search_key, Value_t* value_ptr)
{
	if (skiplist == NULL) return -1;

	struct Tower* preds[NUM_LEVELS];
	struct Tower* succs[NUM_LEVELS];

	// Enter epoch:
	unsigned epoch = enter_epoch(skiplist);

	// Search for the key:
	unsigned start_level = __atomic_load_n(&skiplist->max_level, __ATOMIC_RELAXED);

	fill_tables(skiplist, preds, succs, search_key, start_level, 0);

	struct Tower* cur = succs[0];

	if (cur != NULL && __atomic_load_n(&cur->key, __ATOMIC_RELAXED) == search_key)
	{
		*value_ptr = __atomic_load_n(&cur->value, __ATOMIC_RELAXED);

		// Leave epoch:
		leave_epoch(skiplist, epoch);
		return 0;
	}

	leave_epoch(skiplist, epoch);

	skiplist->errno = NO_ELEMENT;
	return -1;
}

//-------------------------
// Skiplist error-checking 
//-------------------------

int skiplist_get_errno(struct SkipList* skiplist)
{
	return skiplist->errno;
}

void skiplist_print_error(struct SkipList* skiplist)
{
	switch (skiplist->errno)
	{
		case NO_ERROR:
			return;
		case NO_MEMORY:
			fprintf(stderr, MAGENTA"[Error] Unable to allocate memory\n"RESET);
			return;
		case NO_ELEMENT:
			fprintf(stderr, MAGENTA"[Error] Unable to find element by key\n"RESET);
			return;
		default:
			fprintf(stderr, MAGENTA"[Error] Unknown error number\n"RESET);
	}
}
