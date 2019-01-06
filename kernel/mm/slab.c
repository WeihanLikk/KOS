#include <arch.h>
#include <driver/vga.h>
#include <kos/mm/slab.h>
#include <kos/utils.h>

#define KMEM_ADDR( PAGE, BASE ) ( ( ( ( PAGE ) - ( BASE ) ) << PAGE_SHIFT ) | 0x80000000 )

/*
 * one list of PAGE_SHIFT(now it's 12) possbile memory size
 * 96, 192, 8, 16, 32, 64, 128, 256, 512, 1024, (2 undefined)
 * in current stage, set (2 undefined) to be (4, 2048)
 */
struct kmem_cache kmalloc_caches[ PAGE_SHIFT ];

static unsigned int size_kmem_cache[ PAGE_SHIFT ] = { 96, 192, 8, 16, 32, 64, 128, 256, 512, 1024, 1536, 2048 };

static struct kmem_cache *cache_chain;

static unsigned int obj_offset( struct kmem_cache *cachep )
{
	return cachep->obj_offset;
}

static unsigned int obj_size( struct kmem_cache *cachep )
{
	return cachep->obj_size;
}
/*we put slab inside page*/
static struct slab *page_set_slab( struct page *page )
{
	unsigned char *moffset = (unsigned char *)KMEM_ADDR( page, pages );  // physical addr

	set_flag( page, BUDDY_SLAB );

	struct slab *pslab = (struct slab *)moffset;
	void *ptr = moffset + sizeof( struct slab );

	pslab->s_mem = ptr;
	pslab->inuse = 0;
	pslab->flag = 0;
	return pslab;
}

static struct slab *page_get_slab( struct page *page )
{
	unsigned char *moffset = (unsigned char *)KMEM_ADDR( page, pages );  // physical addr
	struct slab *pslab = (struct slab *)moffset;
	return pslab;
}

static void page_set_cache( struct page *page, struct kmem_cache *cache )
{
	struct slab *pslab = page_set_slab( page );

	cache->cpu.page = page;
	page->virtual = (void *)cache;  // Used in k_free
	page->slabp = 0;				// Point to the free-object list in this page
	pslab->colouroff = 0;
	pslab->free = 0;  // Point to the first free-object in this slab
}

static struct kmem_cache *page_get_cache( struct page *page )
{
	return (struct kmem_cache *)page->virtual;  // Used in k_free
}

//color can be 0,1,2
static int cache_set_colornum( struct kmem_cache *cache, unsigned int colornum )
{
#ifdef SLAB_DEBUG
	kernel_printf( "in cache_set_colornum: %x\n", colornum );
#endif
	if ( colornum > COLOR_NUM )
	{
#ifdef SLAB_DEBUG
		kernel_printf( "color num is too big: %x\n", colornum );
#endif
		return 0;
	}
	int allign = cache->colour_off / colornum;
	if ( !allign && colornum != 1 )
	{
#ifdef SLAB_DEBUG
		kernel_printf( "color num is too big: %x\n", colornum );
#endif
		return 0;
	}
	cache->colour = colornum;
	return 1;
}

//color can be 0,1,2
static int page_set_coloroff( struct page *page, unsigned int color )
{
	struct kmem_cache *cache = page_get_cache( page );
	if ( color >= cache->colour )
	{
#ifdef SLAB_DEBUG
		kernel_printf( "color is beyond the upper bound: %x >= %x\n", color, cache->colour );
#endif
		return -1;
	}
	struct slab *pslab = page_get_slab( page );
	pslab->colouroff = color * ( cache->colour_off / cache->colour );
	return 1;
}

// init the struct kmem_cache_cpu
void init_kmem_cpu( struct kmem_cache_cpu *kcpu )
{
	kcpu->page = 0;
	kcpu->freeobj = 0;
}

// init the struct kmem_cache_list3
void init_kmem_list3( struct kmem_cache_list3 *p_list3 )
{
	INIT_LIST_HEAD( &( p_list3->full ) );
	INIT_LIST_HEAD( &( p_list3->partial ) );
	INIT_LIST_HEAD( &( p_list3->empty ) );
	p_list3->colour_next = 0;
}

void init_cache( struct kmem_cache *cache, unsigned int objsize, unsigned int colournum )
{
	cache->obj_size = UPPER_ALLIGN( objsize, SIZE_INT );
	cache->colour = COLOR_NUM;
	cache->colour_off = cache->obj_size - objsize;
	cache->total_size = UPPER_ALLIGN( objsize, SIZE_INT ) + sizeof( void * );

	cache->obj_offset = cache->total_size;

	int k = cache_set_colornum( cache, colournum );
	if ( !k )
	{
		kernel_printf( "in init_cache: set color fail, color: %x\n", colournum );
	}
	init_kmem_cpu( &( cache->cpu ) );
	init_kmem_list3( &( cache->list3 ) );
	cache->next = 0;
}

void init_slab()
{
	unsigned int i;
	struct kmem_cache *tmp_cache;
	cache_chain = tmp_cache;
	kernel_printf( "cache_chain = tmp_chache\n" );
	for ( i = 0; i < PAGE_SHIFT; i++ )
	{
		init_cache( &( kmalloc_caches[ i ] ), size_kmem_cache[ i ], 1 );
		kernel_printf( "in for\n" );
		tmp_cache->next = &( kmalloc_caches[ i ] );
		tmp_cache = &( kmalloc_caches[ i ] );
	}
#ifdef SLAB_DEBUG
	kernel_printf( "Setup Slub ok :\n" );
	kernel_printf( "\tcurrent slab cache size list:\n\t" );
	for ( i = 0; i < PAGE_SHIFT; i++ )
	{
		kernel_printf( "%x %x ", kmalloc_caches[ i ].obj_size, (unsigned int)( &( kmalloc_caches[ i ] ) ) );
	}
	kernel_printf( "\n" );
#endif  // ! SLAB_DEBUG
}

void *slab_alloc( struct kmem_cache *cache )
{
	void *object = 0;
	struct page *cur_page;
	struct slab *pslab;

	if ( !cache->cpu.page ) goto PageFull;

	cur_page = cache->cpu.page;
	pslab = (struct slab *)KMEM_ADDR( cur_page, pages );
#ifdef SLAB_DEBUG
	kernel_printf( "was_full?:%d\n", pslab->flag );
#endif

FromFreeList:
	if ( cur_page->slabp != 0 )
	{  // Allocate from free list
		object = (void *)cur_page->slabp;
		cur_page->slabp = *(unsigned int *)cur_page->slabp;
		++( pslab->inuse );
#ifdef SLAB_DEBUG
		kernel_printf( "From Free-list\nnr_objs:%d\tobject:%x\tnew slabp:%x\n",
					   pslab->inuse, object, cur_page->slabp );
		// kernel_getchar();
#endif  // ! SLAB_DEBUG
		return object;
	}

PageNotFull:
	if ( !pslab->flag )
	{  // Still has uninitialized space
		object = pslab->s_mem;
		pslab->s_mem = object + cache->total_size;
		( pslab->inuse )++;

		if ( pslab->s_mem + cache->total_size - (void *)pslab >= 1 << PAGE_SHIFT )
		{
			pslab->flag = 1;
			list_add_tail( &( cur_page->lru ), &( cache->list3.full ) );

#ifdef SLAB_DEBUG
			kernel_printf( "Become full\n" );
			// kernel_getchar();
#endif  // ! SLAB_DEBUG
		}
#ifdef SLAB_DEBUG
		kernel_printf( "Page not full\nnr_objs:%d\tobject:%x\tend_ptr:%x\n",
					   pslab->inuse, object, pslab->s_mem );
		// kernel_getchar();
#endif  // ! SLAB_DEBUG
		return object;
	}

PageFull:
#ifdef SLAB_DEBUG
	kernel_printf( "Page full\n" );
	// kernel_getchar();
#endif  // ! SLAB_DEBUG

	if ( list_empty( &( cache->list3.partial ) ) )
	{  // No partial pages
		// call the buddy system to allocate one more page to be slab-cache
		cur_page = __alloc_pages( 0 );  // get bplevel = 0 page === one page
		if ( !cur_page )
		{
			// allocate failed, memory in system is used up
			kernel_printf( "ERROR: slab request one page in cache failed\n" );
			while ( 1 )
				;
		}
#ifdef SLAB_DEBUG
		// kernel_printf("\tnew page, index: %x \n", cur_page - pages);
#endif  // ! SLAB_DEBUG

		// using standard format to shape the new-allocated page,
		// set the new page to be cpu.page
		page_set_cache( cur_page, cache );
		pslab = (struct slab *)KMEM_ADDR( cur_page, pages );
		goto PageNotFull;
	}

// Get a partial page
#ifdef SLAB_DEBUG
	kernel_printf( "Get partial page\n" );
#endif
	cache->cpu.page = container_of( cache->list3.partial.next, struct page, lru );
	cur_page = cache->cpu.page;
	list_del( cache->list3.partial.next );
	pslab = (struct slab *)KMEM_ADDR( cur_page, pages );
	goto FromFreeList;
}

void slab_free( struct kmem_cache *cache, void *object )
{
	struct page *opage = pages + ( (unsigned int)object >> PAGE_SHIFT );
	unsigned int *ptr;
	struct slab *pslab = (struct slab *)KMEM_ADDR( opage, pages );
	unsigned char is_full;

	if ( !( pslab->inuse ) )
	{
		kernel_printf( "ERROR : slab_free error!\n" );
		// die();
		while ( 1 )
			;
	}
	object = (void *)( (unsigned int)object | KERNEL_ENTRY );

#ifdef SLAB_DEBUG
	kernel_printf( "page addr:%x\nobject:%x\nslabp:%x\n",
				   opage, object, opage->slabp );
#endif  //SLAB_DEBUG

	is_full = ( !opage->slabp ) && pslab->flag;
	*(unsigned int *)object = opage->slabp;
	opage->slabp = (unsigned int)object;
	( pslab->inuse )--;

#ifdef SLAB_DEBUG
	kernel_printf( "pslab->inuse:%d\tslabp:%x\n", pslab->inuse, opage->slabp );
	// kernel_getchar();
#endif  //SLAB_DEBUG

	if ( list_empty( &( opage->lru ) ) )  // It's CPU
		return;

#ifdef SLAB_DEBUG
	kernel_printf( "Not CPU\n" );
#endif  //SLAB_DEBUG

	if ( !( pslab->inuse ) )
	{
		list_del_init( &( opage->lru ) );
		__free_pages( opage, 0 );  // 也可以不释放？
		return;
#ifdef SLAB_DEBUG
		kernel_printf( "Free\n" );
#endif  //SLAB_DEBUG
	}

	if ( is_full )
	{
		list_del_init( &( opage->lru ) );
		list_add_tail( &( opage->lru ), &( cache->list3.partial ) );
	}
}

// find the best-fit slab system for (size)
unsigned int get_cache( unsigned int size )
{
	unsigned int itop = PAGE_SHIFT;
	unsigned int i;
	unsigned int bf_num = ( 1 << ( PAGE_SHIFT - 1 ) );  // half page
	unsigned int bf_index = PAGE_SHIFT;					// record the best fit num & index

	for ( i = 0; i < itop; i++ )
	{
		if ( ( kmalloc_caches[ i ].obj_size >= size ) && ( kmalloc_caches[ i ].obj_size < bf_num ) )
		{
			bf_num = kmalloc_caches[ i ].obj_size;
			bf_index = i;
		}
	}
	return bf_index;
}

struct kmem_cache *get_cache_with_create( unsigned int size, unsigned int color )
{
	unsigned int itop = PAGE_SHIFT;
	unsigned int i;
	unsigned int bf_num = ( 1 << ( PAGE_SHIFT - 1 ) );  // half page
	unsigned int bf_index = PAGE_SHIFT;					// record the best fit num & index
	struct kmem_cache *tmp_cache;
	tmp_cache = cache_chain->next;
	while ( tmp_cache )
	{
		if ( tmp_cache->obj_size == size )
		{
			return tmp_cache;
		}
	}
	// no cache of the suitable size
	return kmem_cache_create( "new_cache", size, color );
}

void *phy_kmalloc_without_create_cache( unsigned int size )
{
	struct kmem_cache *cache;
	unsigned int bf_index;

	if ( !size )
		return 0;

	// if the size larger than the max size of slab system, then call buddy to
	// solve this
	if ( size > kmalloc_caches[ PAGE_SHIFT - 1 ].obj_size )
	{
		size = UPPER_ALLIGN( size, 1 << PAGE_SHIFT );
		// size += (1 << PAGE_SHIFT) - 1;
		// size &= ~((1 << PAGE_SHIFT) - 1);
		return alloc_pages( size >> PAGE_SHIFT );
	}

	bf_index = get_cache( size );
	if ( bf_index >= PAGE_SHIFT )
	{
		kernel_printf( "ERROR: No available slab\n" );
		while ( 1 )
			;
	}
	return slab_alloc( &( kmalloc_caches[ bf_index ] ) );
}

void *phy_kmalloc_with_create_cache( unsigned int size, unsigned int color )
{
	struct kmem_cache *cache;
	unsigned int bf_index;

	if ( !size )
		return 0;

	// if the size larger than the max size of slab system, then call buddy to
	// solve this
	if ( size > kmalloc_caches[ PAGE_SHIFT - 1 ].obj_size )
	{
		size = UPPER_ALLIGN( size, 1 << PAGE_SHIFT );
		// size += (1 << PAGE_SHIFT) - 1;
		// size &= ~((1 << PAGE_SHIFT) - 1);
		return alloc_pages( size >> PAGE_SHIFT );
	}

	cache = get_cache_with_create( size, color );
	if ( !cache )
	{
		kernel_printf( "ERROR: get_cache_with_create fail\n" );
		while ( 1 )
			;
	}

	return slab_alloc( cache );
}

void *kmalloc( unsigned int size )
{
	void *result;

	if ( !size )
		return 0;

	result = phy_kmalloc_without_create_cache( size );

	if ( result )
		return (void *)( KERNEL_ENTRY | (unsigned int)result );
	else
		return 0;
}

void kfree( void *obj )
{
	struct page *page;

	obj = (void *)( (unsigned int)obj & ( ~KERNEL_ENTRY ) );
	page = pages + ( (unsigned int)obj >> PAGE_SHIFT );
	if ( !( page->flag == BUDDY_SLAB ) )
		return free_pages( (void *)( (unsigned int)obj & ~( ( 1 << PAGE_SHIFT ) - 1 ) ), page->bplevel );

	return slab_free( page->virtual, obj );
}

struct kmem_cache *kmem_cache_create( const char *name, unsigned int objsize, unsigned int colour )
{
	if ( !name )
	{
#ifdef SLAB_DEBUG
		kernel_printf( "Error: no name.\n" );
#endif
		return 0;
	}
	if ( !objsize || objsize > ( 1 << PAGE_SHIFT ) )
	{
#ifdef SLAB_DEBUG
		kernel_printf( "Error: objsize %x.\n", objsize );
#endif
		return 0;
	}
	struct kmem_cache *new_cache = kmalloc( sizeof( struct kmem_cache ) );
	if ( !new_cache )
	{
#ifdef SLAB_DEBUG
		kernel_printf( "kmem_cache_create Error: kmalloc fail.\n" );
#endif
		return 0;
	}
	init_cache( new_cache, objsize, colour );
	return new_cache;
}

static void page_off_cache( struct page *page, struct kmem_cache *cache )
{
	struct slab *pslab = page_set_slab( page );

	page->virtual = 0;  // Used in k_free
	page->slabp = 0;	// Point to the free-object list in this page
}

void kmem_cache_delete( struct kmem_cache *cache )
{
	struct page *page = cache->cpu.page;
	slab_free( cache, (void *)page );
	cache = (void *)( (unsigned int)cache & ( ~KERNEL_ENTRY ) );
	page = pages + ( (unsigned int)cache >> PAGE_SHIFT );
	if ( !( page->flag == BUDDY_SLAB ) )
		free_pages( (void *)( (unsigned int)cache & ~( ( 1 << PAGE_SHIFT ) - 1 ) ), page->bplevel );

	slab_free( (struct kmem_cache *)page->virtual, (void *)cache );
}

void *kmalloc_object_pool( unsigned int size, unsigned int color )
{
	void *result;

	if ( !size )
		return 0;

	result = phy_kmalloc_with_create_cache( size, color );

	if ( result )
		return (void *)( KERNEL_ENTRY | (unsigned int)result );
	else
		return 0;
}

void kfree_object_pool( void *obj )
{
	struct page *page;

	obj = (void *)( (unsigned int)obj & ( ~KERNEL_ENTRY ) );
	page = pages + ( (unsigned int)obj >> PAGE_SHIFT );
	if ( !( page->flag == BUDDY_SLAB ) )
		return free_pages( (void *)( (unsigned int)obj & ~( ( 1 << PAGE_SHIFT ) - 1 ) ), page->bplevel );

	return slab_free( page->virtual, obj );
}