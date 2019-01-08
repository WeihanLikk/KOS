#include <kos/vm/vmm.h>
#include <kos/mm/page.h>
#include <page.h>
#include <kos/mm/slab.h>
#include <kos/utils.h>
#include <kos/pc/sched.h>
#include <driver/vga.h>

#define MM_VADDR 0
#define PGD_VADDR 0x1000

struct mm_struct *mm_create()
{
	struct mm_struct *mm;
	/*kmalloc or vmalloc the mm of this process*/
	mm = kmalloc( sizeof( *mm ) );
// mm = vmalloc(sizeof(*mm));
#ifdef VM_DEBUG
	kernel_printf( "mm_create: %x\n", mm );
#endif  //VM_DEBUG
	if ( !mm )
	{
#ifdef VM_DEBUG
		kernel_printf( "mm_create fail\n" );
#endif  //VM_DEBUG
		return 0;
	}
	kernel_memset( mm, 0, sizeof( *mm ) );  //set zero
	// memset(mm, 0, sizeof(*mm));
	/*kmalloc or vmalloc the page table of this process*/
	// mm->pgd = kmalloc(PAGE_SIZE);
	// mm->pgd = vmalloc(PAGE_SIZE);
	mm->pgd = (pgd_t*)PGD_VADDR;
	if ( !mm->pgd )
	{
#ifdef VM_DEBUG
		kernel_printf( "mm_create fail\n" );
#endif  //VM_DEBUG
		kfree( mm );
		// vfree(mm);
		return 0;
	}

#ifdef VM_DEBUG
	kernel_printf( "mm_create success\n" );
#endif  //VM_DEBUG
	// kernel_memset(mm->pgd, 0, PAGE_SIZE); //set zero to page table
	memset( mm->pgd, 0, PAGE_SIZE );  //ask a space in virtual address space
#ifdef VM_DEBUG
	kernel_printf( "ask a space in virtual address space success\n" );
#endif  //VM_DEBUG
	mm->mmap = 0;
	mm->mmap_cache = 0;
	mm->mm_rb = 0;
	mm->map_count = 0;
	mm->mm_users = 1;
	mm->mm_users = 1;

	return mm;
}

void *memset( void *dest, int b, int len )
{
#ifdef MEMSET_DEBUG
	kernel_printf( "memset:%x,%x,len%x,", (int)dest, b, len );
#endif  // ! MEMSET_DEBUG
	char content = b ? -1 : 0;
	char *deststr = dest;
#ifdef MEMSET_DEBUG
	kernel_printf( "deststr, content:%x\n" ,content);
#endif  // ! MEMSET_DEBUG
	while ( len-- )
	{
#ifdef MEMSET_DEBUG
	kernel_printf( "in while\n");
#endif  // ! MEMSET_DEBUG
		*deststr = content;  //will cause tlb miss
		deststr++;
	}
#ifdef MEMSET_DEBUG
	kernel_printf( "%x\n", (int)deststr );
#endif  // ! MEMSET_DEBUG
	return dest;
}

// Free all the vmas
static void exit_mmap( struct mm_struct *mm )
{
	struct vm_area_struct *vmap = mm->mmap;
	struct vm_area_struct *next_vmap;
	// Node * node, next_node;
	// node = mm->mm_rb->node;
	mm->mmap = mm->mmap_cache = 0;
	while ( vmap )
	{
		next_vmap = vmap->vm_next; /*list implement*/
		// kfree(vmap);
		// vfree(vmap);
		mm->map_count--;
		vmap = next_vmap;
	}
	if ( mm->map_count )
	{
		kernel_printf( "exit mmap bug! %d vma left", mm->map_count );
		//BUG
		while ( 1 )
			;
	}
}

static void pgd_delete( pgd_t* pgd ){
	int i;
	pte_t *pte;
	unsigned int ptec;  //page table entry

	for ( i = 0; i < 1024; i++ )
	{
		ptec = (unsigned int)pgd + i * sizeof( pte_t );
		ptec &= PAGE_MASK;
		pte = (pte_t *)ptec;
#ifdef VMA_DEBUG
		kernel_printf( "Delete pte: %x\n", pte );
#endif  //VMA_DEBUG
		unsigned int pfn = pte->EntryLo0.PFN;
		pfn = pfn << PAGE_SHIFT;
		if ( !pte->EntryLo0.PFN )
			continue;
		free_pages( (void *)pfn, 0 );
#ifdef VM_DEBUG
		kernel_printf( "free physical page done\n" );
#endif  //VM_DEBUG \
		// vfree((void*)pte);
	}
	// kfree(pgd);
	// vfree(pgd);
#ifdef VM_DEBUG
	kernel_printf( "pgd_delete done\n" );
#endif  //VM_DEBUG
	return;
}

//pgd_delete have been implemented
void mm_delete( struct mm_struct *mm )
{
#ifdef VM_DEBUG
	kernel_printf( "mm_delete: pgd %x\n", mm->pgd );
#endif  //VMA_DEBUG
	pgd_delete( mm->pgd );
	exit_mmap( mm );
#ifdef VM_DEBUG
	kernel_printf( "exit_mmap done\n" );
#endif  //VMA_DEBUG
	kfree( mm );
}