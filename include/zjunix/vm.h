#ifndef _ZJUNIX_VM_H
#define _ZJUNIX_VM_H
#include <page.h>
#include <zjunix/page.h>

#define atomic_t unsigned long
#define pgprot_t unsigned long
#define USER_CODE_ENTRY 0x00100000
#define USER_DATA_ENTRY 0x01000000
#define USER_DATA_END
#define USER_BRK_ENTRY 0x10000000
#define USER_STACK_ENTRY 0x80000000
#define USER_DEFAULT_ATTR 0x0f

#ifndef NULL
#define NULL 0
#endif

#define RED 0	// 红色节点
#define BLACK 1  // 黑色节点

/* 红黑树的节点*/
typedef struct RBTreeNode
{
	int color;  // 颜色(RED 或 BLACK) 0 or 1
	struct vm_area_struct *vma;
	struct RBTreeNode *left;	// 左孩子
	struct RBTreeNode *right;   // 右孩子
	struct RBTreeNode *parent;  // 父结点
} Node, *RBTree;

struct mm_rb_root
{
	// 红黑树的根
	Node *node;
};
// 红黑树的节点
/*将rb_node加入红黑树root中,插入成功，返回0；失败返回-1
  have considered the situation that root has not mount any node.*/
int insert_rbtree( struct mm_rb_root *root, Node *rb_node );
/*从红黑树root中删除rb_node*/
void delete_rbtree( struct mm_rb_root *root, Node *node );
void insert_rbtree_color( struct mm_rb_root *root, Node *rb_node );

static inline void mm_rb_link_node( Node *node, Node *parent,
									Node **rb_link )
{
	node->color = RED;
	node->left = node->right = NULL;
	node->parent = parent;
	*rb_link = node;
}

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
void mm_rb_replace_node( Node *victim, Node *new,
						 struct mm_rb_root *root );

/* Find logical next and previous nodes in a tree */
Node *mm_rb_next( Node *node );
Node *mm_rb_prev( Node *node );
Node *mm_rb_first( struct mm_rb_root *root );
Node *mm_rb_last( struct mm_rb_root *root );

Node *rbTree_search( struct mm_rb_root *root, unsigned long addr );

struct vm_area_struct
{
	struct mm_struct *vm_mm;  // 指向进程的mm_struct结构体
	unsigned long vm_start;   // Start address within vm_mm.
	unsigned long vm_end;	 // The first byte after the end address within vm_mm

	struct vm_area_struct *vm_next;  //linked list of VM areas per task, sorted by address

	unsigned long vm_flags;  // Flags
	pgprot_t vm_page_prot;   // Access permissions of this VMA

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
	/*when pages which are not in pysical memory are accessed*/
	void ( *swapin )( struct vm_area_struct *area );
	void ( *swapout )( struct vm_area_struct *area );
	void ( *protect )( struct vm_area_struct *area );
	void ( *unmap )( struct vm_area_struct *area );
};

unsigned int do_mmap( unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags );
unsigned int do_unmmap( unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags );
int is_in_vma( unsigned long addr );
struct vm_area_struct *find_vma_and_prev( struct mm_struct *mm, unsigned long addr, struct vm_area_struct **prev, unsigned int way );
struct vm_area_struct *find_vma( struct mm_struct *mm, unsigned long addr, int way );

struct mm_struct
{
	/*对进程整个用户空间进行描述*/
	struct vm_area_struct *mmap;		// list of VMAs
	struct vm_area_struct *mmap_cache;  // Latest used VMA
	struct mm_rb_root *mm_rb;			// Red-Black Tree

	unsigned int pgd;   // The Base Address of the Process' Page Didetory
	atomic_t mm_users;  // number of users with this user space
	atomic_t mm_count;  // number of references to "struct mm_struct" (users count as 1)
	int map_count;		// 在进程的整个用户空间中vma的个数

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