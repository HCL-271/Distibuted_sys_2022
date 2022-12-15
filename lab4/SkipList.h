//---------------------
// Types and constants 
//---------------------

typedef long Key_t;
typedef long Value_t;
typedef char bool;

struct SkipList;

//----------------
// Initialization 
//----------------

int skiplist_init(struct SkipList** skiplist);
int skiplist_free(struct SkipList* skiplist);

//----------------
// Element access 
//----------------

int skiplist_insert(struct SkipList* skiplist, Key_t search_key, Value_t value);
int skiplist_remove(struct SkipList* skiplist, Key_t search_key);
int skiplist_search(struct SkipList* skiplist, Key_t search_key, Value_t* value_ptr);

//-------------------------
// Skiplist error-checking 
//-------------------------

enum SkiplistErrno
{
	NO_ERROR           = 0,
	NO_MEMORY          = 1,
	NO_THREADS         = 2,
	NO_ELEMENT         = 3,
	THREAD_LOCAL_ERROR = 4
};

int  skiplist_get_errno  (struct SkipList* skiplist);
void skiplist_print_error(struct SkipList* skiplist);

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
