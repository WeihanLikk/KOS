#ifndef  _ZJUNIX_VMA_H
#define  _ZJUNIX_VMA_H
#include <kos/mm/page.h>

#define atomic_t unsigned long
#define pgprot_t unsigned long
#define  USER_CODE_ENTRY   0x00100000
#define  USER_DATA_ENTRY   0x01000000
#define  USER_DATA_END
#define  USER_BRK_ENTRY     0x10000000
#define  USER_STACK_ENTRY   0x80000000
#define  USER_DEFAULT_ATTR     0x0f

struct vm_area_struct {
	struct mm_struct * vm_mm; // 指向进程的mm_struct结构体
	unsigned long vm_start;		// Start address within vm_mm.
	unsigned long vm_end;		// The first byte after the end address within vm_mm

	struct vm_area_struct *vm_next; //linked list of VM areas per task, sorted by address

	unsigned long vm_flags;		// Flags
	pgprot_t vm_page_prot;		// Access permissions of this VMA

	struct vm_operations_struct * vm_ops; //Function pointers to deal with this struct

	union {
		struct vm_area_struct * vm_next_share;
		struct vm_area_struct * vm_pre_share;
	} shared;

	/* Information about our backing store: */
	unsigned long vm_pgoff;		// Offset (within vm_file) in PAGE_SIZE
	struct file * vm_file;		// File we map to (can be NULL)
	void * vm_private_data;		// was vm_pte (shared mem)

};

struct vm_list_struct {
	struct vm_list_struct	*next;
	struct vm_area_struct	*vma;
};

struct vm_operations_struct{
	void (*open)(struct vm_area_struct * area); 
	void (*close)(struct vm_area_struct * area);
	struct page * (*nopage)(struct vm_area_struct * area, unsigned long address, int *type); 
	/*when pages which are not in pysical memory are accessed*/
	void (*swapin)(struct vm_area_struct * area);
	void (*swapout)(struct vm_area_struct * area);
	void (*protect)(struct vm_area_struct * area);
	void (*unmap)(struct vm_area_struct * area);
};

unsigned long do_mmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags);
unsigned long do_unmmap(unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags);
int is_in_vma(unsigned long addr);

#endif