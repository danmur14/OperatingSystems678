/**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 0

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "buddy.h"
#include "list.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MIN_ORDER 12
#define MAX_ORDER 20

#define PAGE_SIZE (1<<MIN_ORDER)
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)((page_idx*PAGE_SIZE) + g_memory)

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)((void *)addr - (void *)g_memory) / PAGE_SIZE)

/* find buddy address */
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)addr - (unsigned long)g_memory) ^ (1<<o)) \
									 + (unsigned long)g_memory)

#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;
	int order;
	int index;
	char *address;
} page_t;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1]; //21

/* memory area */
char g_memory[1<<MAX_ORDER]; //2^20

/* page structures */
page_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE]; //2^20 / 2^12 = 256

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/

void buddy_split(int req_order, int c_order, int r_page)
{
	if (req_order == c_order) //correct size, no need to split
	{
		return;
	}
	else //split pages and assign right to free list, split again
	{
		int r_buddy_index = ADDR_TO_PAGE(BUDDY_ADDR(PAGE_TO_ADDR(r_page), (c_order - 1) )); //get right side page index
		page_t *r_buddy = &g_pages[r_buddy_index]; //right buddy page
		r_buddy->order = c_order - 1; //give right buddy order = current order - 1
		list_add(&r_buddy->list, &free_area[c_order - 1]); //add right buddy to free list
		buddy_split(req_order, c_order - 1, r_page); //continue splitting 
	}
}

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE; //256
	for (i = 0; i < n_pages; i++) {
		INIT_LIST_HEAD(&g_pages[i].list);
		g_pages[i].index = i;
		g_pages[i].order = -1;
		g_pages[i].address = PAGE_TO_ADDR(i);
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) {
		INIT_LIST_HEAD(&free_area[i]);
	}

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);
	g_pages[0].order = MAX_ORDER;
}

/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then splitted into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further splitted while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{
	if (size < 0 || size > 1<<MAX_ORDER)
	{
		return NULL;
	}

	int req_order = ceil(log2(size)) > MIN_ORDER ? ceil(log2(size)) : MIN_ORDER; //get max between required min order or the MIN_ORDER allowed

	for (int temp_order = req_order; temp_order < MAX_ORDER + 1; temp_order++) //starting from the min required order
	{
		if(!list_empty(&free_area[temp_order])) //if list is not empty, found block
		{
			page_t *f_page = list_entry(free_area[temp_order].next, page_t, list); //find the first non-empty free-list with order >= required-order
			int f_page_index = f_page->index; //get page index
			f_page->order = req_order; //assign requested order to free page
			list_del_init(&f_page->list); //remove from free area

			//split pages
			buddy_split(req_order, temp_order, f_page_index);

			//return block
			return PAGE_TO_ADDR(f_page_index);
		}
	}

	return NULL; //could not find free page from list, error
}

/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)
{
	page_t *freed_page = &g_pages[ADDR_TO_PAGE(addr)];
	int c_order = freed_page->order;

	while (c_order <= MAX_ORDER)
	{
		if(c_order == MAX_ORDER) //max order, can't merge anymore
		{
			freed_page->order = c_order; //change page order
			list_add(&freed_page->list, &free_area[c_order]); //add to free list at current order 20
			return;
		}
		else //check if buddy is free
		{
			int buddy_index = ADDR_TO_PAGE(BUDDY_ADDR(PAGE_TO_ADDR(freed_page->index), c_order)); //get buddy index
			page_t *buddy = &g_pages[buddy_index]; //get buddy page
			int b_free = 0; //no bool, check if buddy was found

			struct list_head *pos; //iterator
			list_for_each(pos, &free_area[c_order]) //iterate over list in free area of current order
			{
				if (buddy == list_entry(pos, page_t, list)) //check if free block is buddy
				{
					b_free = 1; //buddy found
				}
			}

			if (b_free) //freed page's buddy is also free
			{
				list_del_init(&buddy->list); //remove from free area
				if(buddy->address < freed_page->address) //get leftmost/first buddy for next iteration
				{
					freed_page = buddy;
				}
				c_order++; //increase order
			}
			else //could not find free buddy
			{
				freed_page->order = c_order; //change page order
				list_add(&freed_page->list, &free_area[c_order]); //add to free list at current order 20
				return;
			}
		}
	}
}

/**
 * Print the buddy system status---order oriented
 *
 * print free pages in each order.
 */
void buddy_dump()
{
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
