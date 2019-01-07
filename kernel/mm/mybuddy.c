#include <driver/vga.h>
#include <kos/utils.h>
#include <kos/bootmm.h>
#include <kos/buddy.h>
#include <kos/list.h>
#include <kos/lock.h>

#define MAX_ORDER  (LIST_NUM -1)

unsigned int kernel_start_pfn, kernel_end_pfn;

struct page *pages;
struct buddy_sys buddy;

/*Display buddy info,including start_pfn, end_pfn...*/
/*In our system start pfn should be 0, end_pfn should be 32767 */
void buddy_info() {
    unsigned int index;
    kernel_printf("Buddy-system :\n");
    kernel_printf("\tstart page-frame number : %x\n", buddy.buddy_start_pfn);
    kernel_printf("\tend page-frame number : %x\n", buddy.buddy_end_pfn);
    for (index = 0; index < LIST_NUM; ++index) {
        kernel_printf("\t#(%x) order list: %x frees\n", index, buddy.free_area[index].nr_free);
    }
}

// Init page struct for all memory
static void init_pages(unsigned int start_pfn, unsigned int end_pfn) {
    unsigned int i;
    for (i = start_pfn; i < end_pfn; i++) {
        set_flag(pages + i, BUDDY_RESERVED);
        (pages + i)->reference = 1;
        (pages + i)->virtual = (void *)(-1);
        (pages + i)->bplevel = 0;
        (pages + i)->slabp = 0;  // initially, the free space is the whole page
        INIT_LIST_HEAD(&(pages[i].lru)); // if there is a * inside, must INIT
    }
}

// Init buddy system
void init_buddy() {
    unsigned int page_size = sizeof(struct page);
    unsigned char *bp_base;
    unsigned int i;

    bp_base = bootmm_alloc_pages(page_size * bmm.max_pfn, _MM_KERNEL, 1 << PAGE_SHIFT);
    // the maxinum number of struct page is max_pfn
    if (!bp_base) {
        // the remaining memory must be large enough to allocate the whole group
        // of buddy page struct,now not enough memory to allocate the whole group
        // of buddy page struct
        kernel_printf("\nERROR : bootmm_alloc_pages failed!\nInit buddy system failed!\n");
        while (1)
            ;
    }

    // Get virtual address for pages array
    pages = (struct page *)((unsigned int)bp_base | 0x80000000);

    init_pages(0, bmm.max_pfn);

    kernel_start_pfn = 0;
    unsigned int kernel_end=0;
    for (i = 0; i < bmm.cnt_infos; ++i) {
        if (bmm.info[i].end > kernel_end)
            kernel_end = bmm.info[i].end;
    }
    kernel_end_pfn = kernel_end >> PAGE_SHIFT;

    // Buddy system occupies the space after kernel part
    buddy.buddy_start_pfn = UPPER_ALLIGN(kernel_end_pfn, 1<< MAX_ORDER);
    buddy.start_page = pages + buddy.buddy_start_pfn;
    // Remain some pages for I/O
    buddy.buddy_end_pfn = bmm.max_pfn & ~((1 << MAX_ORDER) - 1);

    // Init freelists of all bplevels
    for (i = 0; i < MAX_ORDER + 1; i++) {
        buddy.free_area[i].nr_free = 0;
        INIT_LIST_HEAD(&(buddy.free_area[i].free_list));
    }

    init_lock(&(buddy.lock));
   
    for (i = buddy.buddy_start_pfn; i < buddy.buddy_end_pfn; i++) {
        __free_pages(pages + i, 0);
    }
}


/* keep merging buddys until it cannot */
void __free_pages(struct page *pbpage, unsigned int bplevel) {
    /* page_idx -> the current page
     * pair_idx -> the pair of current page
     */
    unsigned int order;
    unsigned int page_idx, buddy_idx;
    unsigned int combined_idx, tmp;
    struct page *buddy_page;

    lockup(&buddy.lock);

    page_idx = pbpage - buddy.start_page;

    for(order=bplevel; order< MAX_ORDER; order++) {
        // Find buddy to combine
        buddy_idx = page_idx ^ (1 << order);
        buddy_page = pbpage + (buddy_idx - page_idx);
        #ifdef BUDDY_DEBUG
        kernel_printf("group%x %x\n", (page_idx),buddy_page);
        #endif
        
        if (!_is_same_bplevel(buddy_page, order)) {
            #ifdef BUDDY_DEBUG
            kernel_printf("%x %x\n", buddy_page->bplevel, order);
            #endif
            break;
        }

        if (buddy_page->flag != BUDDY_FREE) {
            break;      // Its buddy has been allocated or reserved
        }
        // Delete buddy from freelist, and combine them
        list_del_init(&buddy_page->lru);
        --buddy.free_area[order].nr_free;
        combined_idx = buddy_idx & page_idx;
        // Set the bplevel of block being combined as -1
        if (combined_idx == buddy_idx) 
            set_bplevel(buddy_page, -1);
        else 
            set_bplevel(pbpage, -1);
        pbpage += (combined_idx - page_idx);
        page_idx = combined_idx;
    }
    
    set_bplevel(pbpage, order);
    
    set_flag(pbpage, BUDDY_FREE);
    
    list_add(&(pbpage->lru), &(buddy.free_area[bplevel].free_list));
    #ifdef BUDDY_DEBUG
    kernel_printf("%x %x\n", pbpage->lru.next, buddy.free_area[order].free_list.next);
    #endif
    buddy.free_area[order].nr_free++;
    
    unlock(&buddy.lock);

}

struct page *__alloc_pages(unsigned int bplevel) {
    unsigned int current_order, size;
    struct page *page, *buddy_page;
    struct free_area *freeArea;

    lockup(&buddy.lock);

    // Search free pages
    for (current_order = bplevel; current_order <= MAX_ORDER; ++current_order) {
        freeArea = buddy.free_area + current_order;
        if (!list_empty(&(freeArea->free_list)))
            break;
    }
    if(current_order == LIST_NUM){
        // Not found
        unlock(&buddy.lock);
        return 0;
    }
    /* found free pages*/
    /* goto is a dangerous command, so we avoided it.*/
    page = container_of(freeArea->free_list.next, struct page, lru);
    list_del_init(&(page->lru));
    set_bplevel(page, bplevel);
    set_flag(page, BUDDY_ALLOCED);
    (freeArea->nr_free)--;

    size = 1 << current_order;
    /*change freelists after separating a pair of buddys*/
    while (current_order > bplevel) {
        current_order--;
        freeArea--;
        size >>= 1;
        buddy_page = page + size;
        // Add the ramaining half into free list
        list_add(&(buddy_page->lru), &(freeArea->free_list));
        (freeArea->nr_free)++;
        set_bplevel(buddy_page, current_order);
        set_flag(buddy_page, BUDDY_FREE);
    }

    unlock(&buddy.lock);
    return page;
}

void *alloc_pages(unsigned int nrPage) {
    unsigned int bplevel = 0;
    if (!nrPage)
        return 0;
    while (1<<bplevel < nrPage) {
        bplevel ++;
    }
    struct page *page = __alloc_pages(bplevel);

    if (!page)
        return 0;

    return (void *)((page - pages) << PAGE_SHIFT);
}

void free_pages(void *addr, unsigned int bplevel) {
    __free_pages(pages + ((unsigned int)addr >> PAGE_SHIFT), bplevel);
}
