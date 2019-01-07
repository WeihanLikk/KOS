#include <kos/vm/mm_rbtree.h>
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