#ifndef _ZJUNIX_SLAB_H
#define _ZJUNIX_SLAB_H

#include <zjunix/list.h>
#include <zjunix/buddy.h>

#define NODES_SHIFT     0
#define MAX_NUMNODES    (1 << NODES_SHIFT)
#define NUM_INIT_LISTS (2 * MAX_NUMNODES + 1)
#define	CACHE_CACHE 0
#define	SIZE_L3 (1 + MAX_NUMNODES)
#define SIZE_INT 4
#define SLAB_AVAILABLE 0x0
#define SLAB_USED 0xff
#define COLOR_NUM 3


/*
 * struct slab
 *
 * Manages the objs in a slab. Placed at the beginning of mem allocated
 * for a slab.
 * Slabs are chained into three list: fully used, partial, fully free slabs.
 */
struct slab{
    //struct list_head list;  //full/partial/empty
    /* 第一个对象的页内偏移*/
    unsigned int colouroff;
	void *s_mem;/* 第一个对象的虚拟地址 *//* including colour offset */
	unsigned int inuse;/*已分配的对象个数*/	/* num of objs active in slab */
	void* free;/* 第一个空闲对象索引*/
    /* needless since only one page per slab*/
	//unsigned short nodeid;
    unsigned short flag; 
};

/*
 * slab pages is chained in this struct
 * @partial keeps the list of un-totally-allocated pages
 * @full keeps the list of totally-allocated pages
 * @empty keeps the list of empty pages
 */
 
struct kmem_cache_list3 {
    struct list_head partial; //部分分配的slab
    struct list_head full; //完全分配的slab
    struct list_head empty; //没有对象被分配
	unsigned int colour_next;	//Per-node cache coloring
}; 

/*
 * current being allocated page unit
 */
struct kmem_cache_cpu {
    void **freeobj;  // points to the free-space head addr inside current page
    struct page *page;
};

/* kmem_cache代表一个结构体的内存池,对应一个内存大小级别的所有相关信息*/
struct kmem_cache {
    unsigned int total_size; //total size
    unsigned char name[16];
    unsigned int colour;	
    unsigned int colour_off;	/* colour offset */
    struct kmem_cache_list3 list3;
    struct kmem_cache_cpu cpu;
    /*
	 * the allocator can add additional fields and/or padding to every object. 
	 * for debug purpose. total_size contains the total
	 * object size including these internal fields, the following two
	 * variables contain the offset to the user object and its size.
	 */
    unsigned int obj_size; //size of objects
    unsigned int obj_offset;
    struct kmem_cache* next;
};

// extern struct kmem_cache kmalloc_caches[PAGE_SHIFT];
extern void init_slab();
extern void *kmalloc(unsigned int size);
extern void *kmalloc_object_pool(unsigned int size,unsigned int color);
extern void *phy_kmalloc(unsigned int size);
extern void kfree(void *obj);
extern void kfree_object_pool(void *obj);

extern struct kmem_cache * kmem_cache_create( const char *name, unsigned int objsize, unsigned int colour);
extern void kmem_cache_delete( struct kmem_cache * cache);
extern int kmem_cache_number();
extern void slab_info();
void obj_cache_delete( unsigned int size, unsigned int color );

#endif
