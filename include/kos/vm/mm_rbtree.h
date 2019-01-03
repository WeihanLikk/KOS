#ifndef  _ZJUNIX_MM_RBTREE_H
#define  _ZJUNIX_MM_RBTREE_H

#include <kos/vm/vma.h>

#define NULL 0
#define RED        0    // 红色节点
#define BLACK    1    // 黑色节点

typedef struct RBTreeNode{
    // 红黑树的节点
    int color;        // 颜色(RED 或 BLACK) 0 or 1
    struct vm_area_struct *vma;
    struct RBTreeNode *left;    // 左孩子
    struct RBTreeNode *right;    // 右孩子
    struct RBTreeNode *parent;    // 父结点
}Node, *RBTree;

typedef struct rb_root{
    // 红黑树的根
    Node *node;
};

/*将rb_node加入红黑树root中,插入成功，返回0；失败返回-1
  have considered the situation that root has not mount any node.*/
int insert_rbtree(struct rb_root *root, Node * rb_node);
/*从红黑树root中删除rb_node*/
void delete_rbtree(struct rb_root *root, Node * node);
void insert_rbtree_color(struct rb_root *root, Node * rb_node);

static inline void rb_link_node(Node * node, Node * parent,
				Node** rb_link)
{
	node->color = RED;
    node->left = node->right = NULL;
    node->parent = parent;
	*rb_link = node;
}

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
void rb_replace_node(Node *victim, Node * new, 
			    struct rb_root *root);

/* Find logical next and previous nodes in a tree */
Node *rb_next(Node * node);
Node *rb_prev(Node * node);
Node *rb_first(struct rb_root *root);
Node *rb_last(struct rb_root *root);

Node *rbTree_search(struct rb_root* root, unsigned long addr);

#endif