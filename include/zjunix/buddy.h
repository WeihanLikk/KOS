#ifndef _ZJUNIX_BUDDY_H
#define _ZJUNIX_BUDDY_H

#include <zjunix/list.h>
#include <zjunix/lock.h>
#include <zjunix/page.h>

/*
 * order means the size of the set of pages, e.g. order = 1 -> 2^1
 * pages(consequent) are free In current system, we allow the max order to be
 * 4(2^4 consequent free pages)
 */

#define BUDDY_RESERVED (1 << 31)
#define BUDDY_ALLOCED (1 << 30)
#define BUDDY_SLAB (1 << 29)
#define BUDDY_FREE 0
#define MIGRATE_TYPES  5
#define LIST_NUM 8

/*
 * struct buddy page is one info-set for the buddy group of pages
 */
struct page {
    unsigned int flag;       // the declaration of the usage of this page
    unsigned int reference;  // not used for now
    struct list_head lru;   // double-way list with link the page structure of the same bplevel together
    void *virtual;           // slab cache address
    unsigned int bplevel;    // the order level of the page
    unsigned int slabp;      /* if the page is used by slab system,
                              * then slabp represents the base-addr of free space
                              */
};

struct free_area {
   struct list_head  free_list; // Linked list
   unsigned long     nr_free; // Number of free blocks
};


struct buddy_sys {
    unsigned int buddy_start_pfn, buddy_end_pfn;        // Physical frame number of buddy
    struct page *start_page;                            // Address of page array
    struct lock_t lock;                                 // Exclusive clock
    struct free_area free_area[LIST_NUM];      // free areas for each order
};

#define _is_same_bpgroup(page, bage) (((*(page)).bplevel == (*(bage)).bplevel))
#define _is_same_bplevel(page, lval) ((*(page)).bplevel == (lval))
#define set_bplevel(page, lval) ((*(page)).bplevel = (lval))

#define set_ref(page, val) ((*(page)).reference = (val))
#define inc_ref(page, val) ((*(page)).reference += (val))
#define dec_ref(page, val) ((*(page)).reference -= (val))

#define set_flag(page, val) ((*(page)).flag = (val))
#define clean_flag(page, val) ((*(page)).flag = 0)
#define has_flag(page, val) ((*(page)).flag & val)

extern struct page *pages;
extern struct buddy_sys buddy;


extern void __free_pages(struct page *page, unsigned int order);
extern struct page *__alloc_pages(unsigned int order);


extern void free_pages(void *addr, unsigned int order);
extern void *alloc_pages(unsigned int nrPage); //main interface

extern void init_buddy();
extern void buddy_info(); // for debug

#endif