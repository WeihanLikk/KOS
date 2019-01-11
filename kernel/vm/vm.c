#include <zjunix/vm.h>
#include <zjunix/slab.h>
#include <zjunix/utils.h>
#include <zjunix/pc/sched.h>
#include <driver/vga.h>
#include <arch.h>

unsigned long get_unmapped_area( unsigned long addr, unsigned long len, unsigned long flags );
struct vma_struct *find_vma_intersection( struct mm_struct *mm, unsigned long start_addr, unsigned long end_addr );
struct vma_struct *find_vma_prepare( struct mm_struct *mm, unsigned long addr );
void insert_vma_struct( struct mm_struct *mm, struct vma_struct *area );
void exit_mmap( struct mm_struct *mm );
void pgd_delete( pgd_t *pgd );

/* Inplement of RBTree */
static Node *rb_search( Node *node, unsigned long addr )
{
	if ( !node )
	{  //this node has not mounted any vma(normallly impossible)
#ifdef VM_DEBUG
		kernel_printf( "In rb_search: not found suitable node.\n", mm );
#endif  //VM_DEBUG
		return 0;
	}
	struct vm_area_struct *vma = node->vma;
	if ( !vma )
	{  //this node has not mounted any vma(normallly impossible)
#ifdef VM_DEBUG
		kernel_printf( "In rb_search:there is a node not mounted any vma.\n", mm );
#endif  //VM_DEBUG
		return 0;
	}
	if ( vma->vm_end > addr && vma->vm_start <= addr )
	{  //vma is found.
#ifdef VM_DEBUG
		kernel_printf( "In rb_search: vma is found.\n", mm );
#endif  //VM_DEBUG
		return node;
	}
	if ( vma->vm_end <= addr )
		return rb_search( node->right, addr );
	else if ( vma->vm_start > addr )
		return rb_search( node->left, addr );
}

Node *rbTree_search( struct mm_rb_root *root, unsigned long addr )
{
	Node *rnode = root->node;
	if ( rnode )
		return rb_search( rnode, addr );
	else
#ifdef VM_DEBUG
		kernel_printf( "In rbTree_search: mm has no node(vma) mounted.\n", mm );
#endif  //VM_DEBUG
	return 0;
}

static void rbtree_left_rotate( struct mm_rb_root *root, Node *left )
{
	// 设置left的右孩子为right
	Node *right = left->right;

	// 将 “right的左孩子” 设为 “left的右孩子”；
	// 如果right的左孩子非空，将 “left” 设为 “right的左孩子的父亲”
	left->right = right->left;
	if ( right->left != NULL )
		right->left->parent = left;

	// 将 “left的父亲” 设为 “right的父亲”
	right->parent = left->parent;

	if ( left->parent != NULL )
	{
		if ( left->parent->left != left )
			left->parent->right = right;  // 如果left是它父节点的左孩子，则将right设为“x的父节点的左孩子”
		else
			left->parent->left = right;  // 如果left是它父节点的左孩子，则将right设为“x的父节点的左孩子”
	}
	else
	{
		// 如果 “left的父亲” 是空节点，则将right设为根节点
		root->node = right;
	}

	// 将 “left” 设为 “right的左孩子”
	right->left = left;
	// 将 “left的父节点” 设为 right
	left->parent = right;
}

static void rbtree_right_rotate( struct mm_rb_root *root, Node *y )
{
	// 设置x是当前节点的左孩子。
	Node *x = y->left;

	// 将 “x的右孩子” 设为 “y的左孩子”；
	// 如果"x的右孩子"不为空的话，将 “y” 设为 “x的右孩子的父亲”
	y->left = x->right;
	if ( x->right != NULL )
		x->right->parent = y;

	// 将 “y的父亲” 设为 “x的父亲”
	x->parent = y->parent;

	if ( y->parent != NULL )
	{
		if ( y != y->parent->right )
			y->parent->left = x;  // (y是它父节点的左孩子) 将x设为“x的父节点的左孩子”
		else
			y->parent->right = x;  // 如果 y是它父节点的右孩子，则将x设为“y的父节点的右孩子”
	}
	else
	{
		// 如果 “y的父亲” 是空节点，则将x设为根节点
		root->node = x;
	}

	// 将 “y” 设为 “x的右孩子”
	x->right = y;

	// 将 “y的父节点” 设为 “x”
	y->parent = x;
}

static void __rb_erase_color( Node *node, Node *parent,
							  struct mm_rb_root *root )
{
	Node *other;

	while ( ( !node || node->color == BLACK ) && node != root->node )
	{
		if ( parent->left == node )
		{
			other = parent->right;
			if ( other->color == RED )
			{
				other->color = BLACK;
				parent->color = RED;
				rbtree_left_rotate( root, parent );
				other = parent->right;
			}
			if ( ( !other->left ||
				   other->left->color == BLACK ) &&
				 ( !other->right ||
				   other->right->color == BLACK ) )
			{
				other->color = RED;
				node = parent;
				parent = node->parent;
			}
			else
			{
				if ( !other->right ||
					 other->right->color == BLACK )
				{
					register Node *o_left;
					if ( ( o_left = other->left ) )
						o_left->color = BLACK;
					other->color = RED;
					rbtree_right_rotate( root, other );
					other = parent->right;
				}
				other->color = parent->color;
				parent->color = BLACK;
				if ( other->right )
					other->right->color = BLACK;
				rbtree_left_rotate( root, parent );
				node = root->node;
				break;
			}
		}
		else
		{
			other = parent->left;
			if ( other->color == RED )
			{
				other->color = BLACK;
				parent->color = RED;
				rbtree_right_rotate( root, parent );
				other = parent->left;
			}
			if ( ( !other->left ||
				   other->left->color == BLACK ) &&
				 ( !other->right ||
				   other->right->color == BLACK ) )
			{
				other->color = RED;
				node = parent;
				parent = node->parent;
			}
			else
			{
				if ( !other->left ||
					 other->left->color == BLACK )
				{
					register Node *o_right;
					if ( ( o_right = other->right ) )
						o_right->color = BLACK;
					other->color = RED;
					rbtree_left_rotate( root, other );
					other = parent->left;
				}
				other->color = parent->color;
				parent->color = BLACK;
				if ( other->left )
					other->left->color = BLACK;
				rbtree_right_rotate( root, parent );
				node = root->node;
				break;
			}
		}
	}
	if ( node )
		node->color = BLACK;
}

void insert_rbtree_color( struct mm_rb_root *root, Node *rb_node )
{
	Node *parent, *gparent;

	while ( ( parent = rb_node->parent ) && parent->color == RED )
	{
		gparent = parent->parent;

		if ( parent == gparent->left )
		{
			{
				Node *uncle = gparent->right;
				if ( uncle && uncle->color == RED )
				{
					uncle->color = BLACK;
					parent->color = BLACK;
					gparent->color = RED;
					rb_node = gparent;
					continue;
				}
			}

			if ( parent->right == rb_node )
			{
				Node *tmp;
				rbtree_left_rotate( root, parent );
				tmp = parent;
				parent = rb_node;
				rb_node = tmp;
			}

			parent->color = BLACK;
			gparent->color = RED;
			rbtree_right_rotate( root, gparent );
		}
		else
		{
			{
				Node *uncle = gparent->left;
				if ( uncle && uncle->color == RED )
				{
					uncle->color = BLACK;
					parent->color = BLACK;
					gparent->color = RED;
					rb_node = gparent;
					continue;
				}
			}

			if ( parent->left == rb_node )
			{
				Node *tmp;
				rbtree_right_rotate( root, parent );
				tmp = parent;
				parent = rb_node;
				rb_node = tmp;
			}

			parent->color = BLACK;
			gparent->color = RED;
			rbtree_left_rotate( root, gparent );
		}
	}

	root->node->color = BLACK;
}

int insert_rbtree( struct mm_rb_root *root, Node *node )
{
	Node *y = NULL;
	Node *x = root->node;

	//将红黑树当作一颗二叉查找树，将节点添加到二叉查找树中。
	while ( x != NULL )
	{
		y = x;
		if ( node->vma->vm_end < x->vma->vm_start )
			x = x->left;
		else if ( node->vma->vm_start > x->vma->vm_end )
			x = x->right;
		else
		{
#ifdef VM_DEBUG
			kernel_printf( "In insert_rbtree: insert fail1.node: %x %x \n",
						   vma->vm_start, vma->vm_end );
#endif  //VM_DEBUG
			return -1;
		}
	}
	node->parent = y;

	if ( y != NULL )
	{
		if ( node->vma->vm_end < y->vma->vm_start )
			y->left = node;  // 情况2：若“node” < “y”，则将node设为“y的左孩子”
		else if ( node->vma->vm_start > y->vma->vm_end )
			y->right = node;  // 情况3：“node” > “y”, 将node设为“y的右孩子”
		else
		{
#ifdef VM_DEBUG
			kernel_printf( "In insert_rbtree: insert fail2. node: %x %x \n",
						   vma->vm_start, vma->vm_end );
#endif  //VM_DEBUG
			return -1;
		}
	}
	else
	{
		root->node = node;  // 情况1：若y是空节点，则将node设为根
	}

	// 设置节点的颜色为红色
	node->color = RED;

	// 3. 将它重新修正为一颗二叉查找树
	insert_rbtree_color( root, node );
}

void delete_rbtree( struct mm_rb_root *root, Node *node )
{
	Node *child, *parent;
	int color;

	if ( !node->left )
		child = node->right;
	else if ( !node->right )
		child = node->left;
	else
	{
		Node *old = node, *left;

		node = node->right;
		while ( ( left = node->left ) != NULL )
			node = left;
		child = node->right;
		parent = node->parent;
		color = node->color;

		if ( child )
			child->parent = parent;
		if ( parent )
		{
			if ( parent->left == node )
				parent->left = child;
			else
				parent->right = child;
		}
		else
			root->node = child;

		if ( node->parent == old )
			parent = node;
		node->parent = old->parent;
		node->color = old->color;
		node->right = old->right;
		node->left = old->left;

		if ( old->parent )
		{
			if ( old->parent->left == old )
				old->parent->left = node;
			else
				old->parent->right = node;
		}
		else
			root->node = node;

		old->left->parent = node;
		if ( old->right )
			old->right->parent = node;
		goto color;
	}

	parent = node->parent;
	color = node->color;

	if ( child )
		child->parent = parent;
	if ( parent )
	{
		if ( parent->left == node )
			parent->left = child;
		else
			parent->right = child;
	}
	else
		root->node = child;

color:
	if ( color == BLACK )
		__rb_erase_color( child, parent, root );
}

/*
 * This function returns the first node (in sort order) of the tree.
 */
Node *mm_rb_first( struct mm_rb_root *root )
{
	Node *n;

	n = root->node;
	if ( !n )
		return NULL;
	while ( n->left )
		n = n->left;
	return n;
}

Node *mm_rb_last( struct mm_rb_root *root )
{
	Node *n;

	n = root->node;
	if ( !n )
		return NULL;
	while ( n->right )
		n = n->right;
	return n;
}

Node *mm_rb_next( Node *node )
{
	/* If we have a right-hand child, go down and then left as far
	   as we can. */
	if ( node->right )
	{
		node = node->right;
		while ( node->left )
			node = node->left;
		return node;
	}

	/* No right-hand children.  Everything down and left is
	   smaller than us, so any 'next' node must be in the general
	   direction of our parent. Go up the tree; any time the
	   ancestor is a right-hand child of its parent, keep going
	   up. First time it's a left-hand child of its parent, said
	   parent is our 'next' node. */
	while ( node->parent && node == node->parent->right )
		node = node->parent;

	return node->parent;
}

Node *mm_rb_prev( Node *node )
{
	/* If we have a left-hand child, go down and then right as far
	   as we can. */
	if ( node->left )
	{
		node = node->left;
		while ( node->right )
			node = node->right;
		return node;
	}

	/* No left-hand children. Go up till we find an ancestor which
	   is a right-hand child of its parent */
	while ( node->parent && node == node->parent->left )
		node = node->parent;

	return node->parent;
}

void mm_rb_replace_node( Node *victim, Node *new,
						 struct mm_rb_root *root )
{
	Node *parent = victim->parent;

	/* Set the surrounding nodes to point to the replacement */
	if ( parent )
	{
		if ( victim == parent->left )
			parent->left = new;
		else
			parent->right = new;
	}
	else
	{
		root->node = new;
	}
	if ( victim->left )
		victim->left->parent = new;
	if ( victim->right )
		victim->right->parent = new;

	/* Copy the pointers/colour from the victim to the replacement */
	*new = *victim;
}

/************mm_struct************/
struct mm_struct *mm_create()
{
	struct mm_struct *mm;

	mm = kmalloc( sizeof( *mm ) );
#ifdef VMA_DEBUG
	kernel_printf( "mm_create: %x\n", mm );
#endif  //VMA_DEBUG
	if ( mm )
	{
		kernel_memset( mm, 0, sizeof( *mm ) );
		// mm->pgd = (void*)do_mmap(10,PAGE_SIZE,0,0);
		mm->pgd = kmalloc( PAGE_SIZE );
		if ( mm->pgd )
		{
			kernel_memset( mm->pgd, 0, PAGE_SIZE );
			//kernel_printf("after memset\n");
			return mm;
		}
#ifdef VMA_DEBUG
		kernel_printf( "mm_create fail\n" );
#endif  //VMA_DEBUG
		kfree( mm );
	}
	mm->mmap = 0;
	mm->mmap_cache = 0;
	mm->mm_rb = 0;
	mm->map_count = 0;
	mm->mm_users = 1;
	mm->mm_count = 1;
	return 0;
}

void mm_delete( struct mm_struct *mm )
{
#ifdef VMA_DEBUG
	kernel_printf( "mm_delete: pgd %x\n", mm->pgd );
#endif  //VMA_DEBUG
	pgd_delete( mm->pgd );
	exit_mmap( mm );
#ifdef VMA_DEBUG
	kernel_printf( "exit_mmap done\n" );
#endif  //VMA_DEBUG
	kfree( mm );
}

void pgd_delete( pgd_t *pgd )
{
	int i, j;
	unsigned int pde, pte;
	unsigned int *pde_ptr;

	for ( i = 0; i < 1024; i++ )
	{
		pde = pgd[ i ];
		pde &= PAGE_MASK;

		if ( pde == 0 )  //不存在二级页表
			continue;
#ifdef VMA_DEBUG
		kernel_printf( "Delete pde: %x\n", pde );
#endif  //VMA_DEBUG
		pde_ptr = (unsigned int *)pde;
		for ( j = 0; j < 1024; j++ )
		{
			pte = pde_ptr[ j ];
			pte &= PAGE_MASK;
			if ( pte != 0 )
			{
#ifdef VMA_DEBUG
				kernel_printf( "Delete pte: %x\n", pte );
#endif  //VMA_DEBUG
				kfree( (void *)pte );
			}
		}
		kfree( (void *)pde_ptr );
	}
	kfree( pgd );
#ifdef VMA_DEBUG
	kernel_printf( "pgd_delete done\n" );
#endif  //VMA_DEBUG
	return;
}

void *memset( void *dest, int b, int len )
{
#ifdef MEMSET_DEBUG
	kernel_printf( "memset:%x,%x,len%x,", (int)dest, b, len );
#endif  // ! MEMSET_DEBUG
	char content = b ? -1 : 0;
	char *deststr = dest;
#ifdef MEMSET_DEBUG
	kernel_printf( "deststr, content:%x\n", content );
#endif  // ! MEMSET_DEBUG
	while ( len-- )
	{
#ifdef MEMSET_DEBUG
		kernel_printf( "in while\n" );
#endif						 // ! MEMSET_DEBUG
		*deststr = content;  //will cause tlb miss
		deststr++;
	}
#ifdef MEMSET_DEBUG
	kernel_printf( "%x\n", (int)deststr );
#endif  // ! MEMSET_DEBUG
	return dest;
}

/*************VMA*************/
// Find the first vma with ending address greater than addr
// struct vm_area_struct* find_vma(struct mm_struct* mm, unsigned long addr)
// {
//     struct vm_area_struct* vma = 0;

//     if (mm) {
//         vma = mm->mmap_cache;
//         if (vma && vma->vm_end>addr && vma->vm_start<=addr)
//             return vma;
//         vma = mm->mmap;
//         while (vma) {
//             if (vma->vm_end > addr) {
//                 mm->mmap_cache = vma;
//                 break;
//             }
//             vma = vma->vm_next;
//         }
//     }
//     return vma;
// }

// // Find the first vma overlapped with start_addr~end_addr
// struct vm_area_struct* find_vma_intersection(struct mm_struct* mm, unsigned long start_addr, unsigned long end_addr)
// {
//     struct vm_area_struct* vma = find_vma(mm, start_addr);
//     if (vma && end_addr<=vma->vm_start)
//         vma = 0;
//     return vma;
// }

// Return the first vma with ending address greater than addr, recording the previous vma
struct vm_area_struct *find_vma_and_prev( struct mm_struct *mm, unsigned long addr, struct vm_area_struct **prev, unsigned int way )
{  //needless when rb tree is applied
	struct vm_area_struct *vma = 0;
	*prev = 0;
	if ( mm )
	{
		vma = mm->mmap;
		while ( vma )
		{
			if ( vma->vm_end > addr )
			{
				mm->mmap_cache = vma;
				break;
			}
			*prev = vma;
			vma = vma->vm_next;
		}
	}
	return vma;
}

// // Get unmapped area starting after addr        目前省略了file, pgoff
unsigned long get_unmapped_area( unsigned long addr, unsigned long len, unsigned long flags )
{
	int check = 0;
	struct vm_area_struct *vma, *tmp_vma;
	struct mm_struct *mm = current_task->mm;  //全局变量，当前线程对应的task_struct
	Node *node, *tmp_node;
	//addr = UPPER_ALLIGN(addr, PAGE_SIZE);   // Allign to page size
	if ( addr + len > KERNEL_ENTRY ) return -1;
	vma = mm->mmap;
	if ( flags == 0 )
	{  //use link list
		while ( vma )
		{
			if ( vma->vm_end < addr )
			{
				check = 1;
				break;
			}
			vma = vma->vm_next;
			//mm->mmap_cache = vma;
		}
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
	return 0;
}

// // Find vma preceding addr, assisting insertion
// struct vm_area_struct* find_vma_prepare(struct mm_struct* mm, unsigned long addr)
// {
//     struct vm_area_struct* vma = mm->mmap;
//     struct vm_area_struct* prev = 0;
//     while (vma) {
//         if (addr < vma->vm_start) break;
//         prev = vma;
//         vma = vma->vm_next;
//     }
//     return prev;
// }

// // Insert vma to the linked list
// void insert_vm_area_struct(struct mm_struct* mm, struct vm_area_struct* area)
// {
//     struct vm_area_struct* vma = find_vma_prepare(mm, area->vm_start);
// #ifdef VMA_DEBUG
//     kernel_printf("Insert: %x  %x", vma, area->vm_start);
// #endif //VMA_DEBUG
//     if (!vma) {
// #ifdef VMA_DEBUG
//         kernel_printf("mmap init\n");
// #endif //VMA_DEBUG
//         mm->mmap = area;
//         area->vm_next = 0;
//     }
//     else {
//         area->vm_next = vma->vm_next;
//         vma->vm_next = area;
//     }
//     mm->map_count ++;
// }

// Find the first vma with ending address greater than addr
// 搜索第一个vm_end大于addr的内存区域（寻找第一个包含addr或vm_start大于addr的区域）
struct vm_area_struct *find_vma( struct mm_struct *mm, unsigned long addr, int way )
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

struct vm_area_struct *create_vma( unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags )
{
	struct mm_struct *mm = current_task->mm;
	struct vm_area_struct *vma;
	vma = kmalloc( sizeof( struct vm_area_struct ) );
	if ( !vma ) return 0;
	vma->vm_mm = mm;
	vma->vm_start = addr;
	vma->vm_end = UPPER_ALLIGN( addr + len, PAGE_SIZE );
	vma->vm_next = 0;
	vma->vm_page_prot = prot;
	vma->vm_flags = flags;
}

// Mapping a region
unsigned int do_mmap( unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags )
{
	struct mm_struct *mm = current_task->mm;
	struct vm_area_struct *vma, *prev;
	addr = UPPER_ALLIGN( addr, PAGE_SIZE );  // Allign to page size
	Node *node;
	node = kmalloc( sizeof( Node ) );
	if ( !len ) return addr;
	if ( mm->map_count == 0 )
	{
		vma = create_vma( addr, len, prot, flags );
		if ( !vma )
		{
			kernel_printf( "vma create failure!" );
			while ( 1 )
				;
		}
		mm->mmap = vma;
		mm->mmap_cache = vma;
		mm->map_count++;
		node->vma = vma;
		mm->mm_rb->node = node;
		return addr;
	}
	addr = get_unmapped_area( addr, len, flags );
	kernel_printf( "after get_unmapped_area!]\n" );
	vma = create_vma( addr, len, prot, flags );
	if ( !vma )
	{
		kernel_printf( "vma create failure!" );
		while ( 1 )
			;
	}
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
	mm->mmap_cache->vm_next = vma;
	mm->mmap_cache = vma;
	mm->map_count++;  //don't forget to set mm->map_count=0 when init
	node->vma = vma;
	//#ifdef VMA_DEBUG
	//     kernel_printf("Do map: %x  %x\n", vma->vm_start, vma->vm_end);
	// #endif //VMA_DEBUG
	//    insert_vma( mm, node );
	return addr;
}

unsigned int do_unmmap( unsigned long addr, unsigned long len, unsigned long prot, unsigned long flags )
{
	struct mm_struct *mm = current_task->mm;
	struct vm_area_struct *vma, *prev;
	//     Node *node, *next_node;
	//     if (addr>KERNEL_ENTRY || len+addr>KERNEL_ENTRY) return -1;  // Bad addr
	//     if(flags){
	//         	node = rbTree_search( mm->mm_rb, addr );
	// 	if ( !node || !node->vma )  // It has not been mapped
	// 		return 0;
	// #ifdef VM_DEBUG
	// 	kernel_printf( "do_unmap. %x %x\n", addr, vma->vm_start );
	// #endif  //VM_DEBUG
	// 	next_node = mm_rb_next( node );
	// 	if ( !next_node )
	// 	{  //the vma mounted to node is the bigggest
	// 		delete_rbtree( mm->mm_rb, node );
	// 		kfree( vma );
	// 		kfree( node );
	// 		mm->map_count--;
	// 		return 0;
	// 	}
	// 	if ( !next_node->vma )
	// 	{
	// #ifdef VM_DEBUG
	// 		kernel_printf( "In do_unmmap:there is a node not mounted any vma.\n" );
	// #endif  //VM_DEBUG
	// 		return -1;
	// 	}
	// 	while ( node->vma->vm_start > addr + len )
	// 	{
	// 		next_node = mm_rb_next( node );
	// 		delete_rbtree( mm->mm_rb, node );
	// 		kfree( vma );
	// 		kfree( node );
	// 		mm->map_count--;
	// 		node = next_node;
	// 	}
	//     }
	//     vma = find_vma_and_prev(mm, addr, &prev,0);
	//     kernel_printf("after find_vma_and_prev\n");
	//     if (!vma || vma->vm_start>=addr+len) return 0;      // It has not been mapped
	// #ifdef VMA_DEBUG
	//     kernel_printf("do_unmap. %x %x\n", addr, vma->vm_start);
	// #endif //VMA_DEBUG
	//     // VMA Length match
	//     if (vma->vm_end >= addr+len) {
	// #ifdef VMA_DEBUG
	//         kernel_printf("Length match\n");
	// #endif //VMA_DEBUG
	//         if (!prev) {
	//             mm->mmap = vma->vm_next;
	//         }
	//         else {
	//             prev->vm_next = vma->vm_next;
	//         }
	//         kfree(vma);
	//         mm->map_count --;
	// #ifdef VMA_DEBUG
	//         kernel_printf("Unmap done. %d vma(s) left\n", mm->map_count);
	// #endif //VMA_DEBUG
	//         return 0;
	//     }

	//     // Length mismatch
	// #ifdef VMA_DEBUG
	//     kernel_printf("Length match");
	// #endif //VMA_DEBUG
	mm->map_count--;
	return 1;
}

// Free all the vmas
void exit_mmap( struct mm_struct *mm )
{
	struct vm_area_struct *vmap = mm->mmap;
	mm->mmap = mm->mmap_cache = 0;
	while ( vmap )
	{
		struct vm_area_struct *next = vmap->vm_next;
		kfree( vmap );
		mm->map_count--;
		vmap = next;
	}
	if ( mm->map_count )
	{
		kernel_printf( "exit mmap bug! %d vma left", mm->map_count );
		//BUG
		while ( 1 )
			;
	}
}

int is_in_vma( unsigned long addr )
{
	struct mm_struct *mm = current_task->mm;
	struct vm_area_struct *vma = 0;
	vma = find_vma( mm, addr, 0 );
	if ( !vma || vma->vm_start > addr )
		return 0;
	else
		return 1;
}