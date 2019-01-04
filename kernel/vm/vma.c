#include <kos/vm/vmm.h>
#include <kos/mm/page.h>
#include <kos/mm/slab.h>
#include <kos/utils.h>
#include <kos/pc/sched.h>
#include <arch.h>

#define MAP_FIXED 0x0
#define MAP_SHARED 0x1
#define MAP_PRIVATE 0x2
#define PROT_READ 0x1
#define PROT_WEITE 0x2
#define PROT_EXEC 0x4
#define PROT_NONE 0x0

// Find the first vma with ending address greater than addr
static struct vm_area_struct *find_vma( struct mm_struct *mm, unsigned long addr, int way )
{  // way==0，链表实现；way==1，红黑树实现
	struct vm_area_struct *vma = 0;

	if ( !mm )
	{
#ifdef VM_DEBUG
		kernel_printf( "In find_vma: mm does not exist.\n", mm );
#endif  //VM_DEBUG
		return 0;
	}
	//先找cache，若正是，就返回
	vma = mm->mmap_cache;
	if ( vma && vma->vm_end > addr && vma->vm_start <= addr )
		return vma;
	// way==1，红黑树实现
	if ( way == 1 )
	{
		Node *node = rbTree_search( mm->mm_rb, addr );
		if ( node->vma )
			vma = node->vma;
	}
	// way==0，链表实现
	else if ( way == 0 )
	{
		vma = mm->mmap;
		while ( vma )
		{
			if ( vma->vm_end > addr )
			{
				mm->mmap_cache = vma;
				break;
			}
			vma = vma->vm_next;
		}
	}
	return vma;
}

// Find the first vma overlapped with start_addr~end_addr
static struct vm_area_struct *find_vma_intersection( struct mm_struct *mm, unsigned long start_addr,
													 unsigned long end_addr )
{
	struct vm_area_struct *vma = find_vma( mm, start_addr, 1 );
	if ( vma && end_addr <= vma->vm_start )
		vma = 0;
	return vma;
}

// Insert vma to the linked list
void insert_vma( struct mm_struct *mm, Node *node )
{
	insert_rbtree( mm->mm_rb, node );
}

// Get unmapped area starting after addr
// 通过keep tmp_vma巧妙地避免了二次查找，减小了算法的时间复杂度
unsigned long get_unmapped_area( unsigned long addr, unsigned long len, unsigned long prot,
								 unsigned long flags )
{
	struct task_struct *current_task = my_cfs_rq.current_task;
	struct vm_area_struct *vma, *tmp_vma;
	struct mm_struct *mm = current_task->mm;  //全局变量，当前线程对应的task_struct
	Node *node, *tmp_node;

	addr = UPPER_ALLIGN( addr, PAGE_SIZE );					   // Allign to page size
	if ( addr + len >= KERNEL_ENTRY || addr == 0 ) return -1;  // 地址到了内核空间，非法

	if ( mm->map_count == 0 )
	{  //no vma yet
		return addr;
	}

	node = rbTree_search( mm->mm_rb, addr );

	while ( 1 )
	{
		node = tmp_node;  //暂存addr位置对应的node
		vma = node->vma;
		tmp_node = mm_rb_next( node );
		addr = vma->vm_end;
		if ( addr + len >= KERNEL_ENTRY )
			return -1;
		if ( !node )
		{
			tmp_vma = tmp_node->vma;
			if ( tmp_vma->vm_start - vma->vm_end > len )  //则该段可用，返回addr
				return addr;
		}  //否则继续查找
	}
}

// Mapping a region
unsigned long do_mmap( unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags )
{
	struct task_struct *current_task = my_cfs_rq.current_task;
	struct mm_struct *mm = current_task->mm;
	struct vm_area_struct *vma;
	Node *node;
#ifdef VM_DEBUG
	kernel_printf( "Do map..." );
#endif  //VM_DEBUG
	if ( !len ) return addr;
	addr = get_unmapped_area( addr, len, prot, flags );
	node = kmalloc( sizeof( Node ) );
	if ( !node ) return -1;
	vma = kmalloc( sizeof( struct vm_area_struct ) );
	if ( !vma ) return -1;

	vma->vm_mm = mm;
	vma->vm_start = addr;
	vma->vm_end = UPPER_ALLIGN( addr + len, PAGE_SIZE );
	vma->vm_next = 0;
#ifdef VM_DEBUG
	kernel_printf( " %x  %x\n", vma->vm_start, vma->vm_end );
#endif  //VM_DEBUG
	if ( !mm->mmap_cache )
	{
#ifdef VM_DEBUG
		kernel_printf( "creating first vma in this process..\n" );
#endif  //VM_DEBUG
		mm->mmap = vma;
	}
	else if ( mm->mmap_cache->vm_next )
	{
#ifdef VM_DEBUG
		kernel_printf( "In do_mmap: Error!\n" );
#endif  //VM_DEBUG
		while ( 1 )
			;
	}
	else
		mm->mmap_cache->vm_next = vma;
	mm->mmap_cache = vma;
	mm->map_count++;  //don't forget to set mm->map_count=0 when init
	node->vma = vma;
	insert_vma( mm, node );  //
	return addr;
}

unsigned long do_unmap( unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags )
{
	struct task_struct *current_task = my_cfs_rq.current_task;
	struct mm_struct *mm = current_task->mm;
	struct vm_area_struct *vma;
	Node *node, *next_node;
	if ( addr > KERNEL_ENTRY || len + addr > KERNEL_ENTRY )
	{
#ifdef VM_DEBUG
		kernel_printf( "In do_unmap: Bad addr %x  %x\n", vma->vm_start, vma->vm_end );
#endif  //VM_DEBUG
		return -1;
	}
	node = rbTree_search( mm->mm_rb, addr );
	if ( !node || !node->vma )  // It has not been mapped
		return 0;
#ifdef VM_DEBUG
	kernel_printf( "do_unmap. %x %x\n", addr, vma->vm_start );
#endif  //VM_DEBUG
	next_node = mm_rb_next( node );
	if ( !next_node )
	{  //the vma mounted to node is the bigggest
		delete_rbtree( mm->mm_rb, node );
		kfree( vma );
		kfree( node );
		mm->map_count--;
		return 0;
	}
	if ( !next_node->vma )
	{
#ifdef VM_DEBUG
		kernel_printf( "In do_unmmap:there is a node not mounted any vma.\n" );
#endif  //VM_DEBUG
		return -1;
	}
	while ( node->vma->vm_start > addr + len )
	{
		next_node = mm_rb_next( node );
		delete_rbtree( mm->mm_rb, node );
		kfree( vma );
		kfree( node );
		mm->map_count--;
		node = next_node;
	}
	return 0;
}

int is_in_vma( unsigned long addr )
{
	struct task_struct *current_task = my_cfs_rq.current_task;
	struct mm_struct *mm = current_task->mm;
	struct vm_area_struct *vma = find_vma( mm, addr, 1 );
	if ( vma && vma->vm_start < addr )
		return 1;
	else
		return 0;
}