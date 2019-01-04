#ifndef _ZJUNIX_MMTYPES_H
#define _ZJUNIX_MMTYPES_H
#include <kos/mm/page.h>
#include <kos/mm/mm_rbtree.h>

#define atomic_t unsigned long
#define pgprot_t unsigned long
#define USER_CODE_ENTRY 0x00100000
#define USER_DATA_ENTRY 0x01000000
#define USER_DATA_END
#define USER_BRK_ENTRY 0x10000000
#define USER_STACK_ENTRY 0x80000000
#define USER_DEFAULT_ATTR 0x0f

struct mm_struct
{
	/*对进程整个用户空间进行描述*/
	struct vm_area_struct *mmap;		// list of VMAs
	struct vm_area_struct *mmap_cache;  // Latest used VMA (last find_vma result)
	struct mm_rb_root mm_rb;			// Red-Black Tree
	pgd_t *pgd;							// The Base Address of the Process' Page Didetory
	atomic_t mm_users;					// number of users with this user space
	atomic_t mm_count;					// number of references to "struct mm_struct" (users count as 1)
	int map_count;						// 在进程的整个用户空间中vma的个数

	unsigned long start_code, end_code;						//进程的代码段的起始地址和终止地址
	unsigned long start_data, end_data;						//进程的数据段的起始地址和终止地址
	unsigned long start_brk, brk, start_stack;				//堆(空洞)的起始地址和终止的地址, 堆栈段的起始地址
	unsigned long total_vm, locked_vm, shared_vm, exec_vm;  //程所需的总页数，被锁定在物理内存中的页数
	unsigned long rss;										//进程驻留在物理内存中的页面数
	unsigned long arg_start, arg_end, env_start, env_end;

	// struct rw_semaphore mmap_sem;
	// spinlock_t page_table_lock;		/* Protects page tables and some counters */
	// mm_context_t context;     /* Architecture-specific MM context */
};

struct vm_area_struct
{
	struct mm_struct *vm_mm;  // 指向进程的mm_struct结构体
	unsigned long vm_start;   // Start address within vm_mm.
	unsigned long vm_end;	 // The first byte after the end address within vm_mm

	struct vm_area_struct *vm_next;  //linked list of VM areas per task, sorted by address

	unsigned long vm_flags;  // Flags
	pgprot_t vm_page_prot;   // Access permissions of this VMA

	Node vm_rb;  // vm_rb的左指针rb_left指向相邻的低地址虚存段，右指针rb_right指向相邻的高地址虚存段

	struct vm_operations_struct *vm_ops;  //Function pointers to deal with this struct

	union
	{
		struct vm_area_struct *vm_next_share;
		struct vm_area_struct *vm_pre_share;
	} shared;

	/* Information about our backing store: */
	unsigned long vm_pgoff;  // Offset (within vm_file) in PAGE_SIZE
	struct file *vm_file;	// File we map to (can be NULL)
	void *vm_private_data;   // was vm_pte (shared mem)
};

struct vm_list_struct
{
	struct vm_list_struct *next;
	struct vm_area_struct *vma;
};

struct vm_operations_struct
{
	void ( *open )( struct vm_area_struct *area );
	void ( *close )( struct vm_area_struct *area );
	struct page *( *nopage )( struct vm_area_struct *area, unsigned long address, int *type );
	void ( *swapin )( struct vm_area_struct *area );
	void ( *swapout )( struct vm_area_struct *area );
	void ( *protect )( struct vm_area_struct *area );
	void ( *unmap )( struct vm_area_struct *area );
};

void mm_delete( struct mm_struct *mm );
struct mm_struct *mm_create();
struct mm_struct *copy_mm( struct mm_struct *mm );

unsigned long do_map( unsigned long addr, unsigned long len, unsigned long flags );
int do_unmap( unsigned long addr, unsigned long len );
int is_in_vma( unsigned long addr );

extern void set_tlb_asid( unsigned int asid );

#endif