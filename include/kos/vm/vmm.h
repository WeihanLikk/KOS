#ifndef _ZJUNIX_VMM_H
#define _ZJUNIX_VMM_H

#include <page.h>
#include <kos/mm/page.h>
#include <kos/vm/mm_rbtree.h>
struct mm_struct
{
	/*对进程整个用户空间进行描述*/
	struct vm_area_struct *mmap;		// list of VMAs
	struct vm_area_struct *mmap_cache;  // Latest used VMA (last find_vma result)
	struct mm_rb_root *mm_rb;			// Red-Black Tree
	pgd_t *pgd;							// The Base Address of the Process' Page Didetory
	atomic_t mm_users;					// number of users with this user space
	atomic_t mm_count;					// number of references to "struct mm_struct" (users count as 1)
	int map_count;						// 在进程的整个用户空间中vma的个数

	unsigned long start_code, end_code;						//进程的代码段的起始地址和终止地址
	unsigned long start_data, end_data;						//进程的数据段的起始地址和终止地址
	unsigned long start_brk, brk, start_stack;				//堆(空洞)的起始地址和终止的地址, 堆栈段的起始地址
	unsigned long total_vm, locked_vm, shared_vm, exec_vm;  //所需的总页数，被锁定在物理内存中的页数
	unsigned long rss;										//进程驻留在物理内存中的页面数
	unsigned long arg_start, arg_end, evoid, env_start, env_end;

	// struct rw_semaphore mmap_sem;
	// spinlock_t page_table_lock;		/* Protects page tables and some counters */
	// mm_context_t context;     /* Architecture-specific MM context */
};

struct mm_struct *mm_create();
void mm_delete( struct mm_struct *mm );
void *memset( void *dest, int b, int len );

#endif