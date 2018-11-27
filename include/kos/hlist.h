#ifndef _HLIST_H
#define _HLIST_H

static inline void prefetch(const void *x) {;}
static inline void prefetchw(const void *x) {;}
 
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
 
#define container_of(ptr, type, member) ( { \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) ); } )
 

#ifndef LIST_POISON1
#define LIST_POISON1 ((void *) 0x00100100)
#endif
 
#ifndef LIST_POISON2
#define LIST_POISON2 ((void *) 0x00200200)
#endif

//hash桶的头结点 
struct hlist_head { 
     struct hlist_node *first;//指向每一个hash桶的第一个结点的指针 
}; 
//hash桶的普通结点 
struct hlist_node { 
 struct hlist_node *next;//指向下一个结点的指针 
 struct hlist_node **pprev;//指向上一个结点的next指针的地址 
}; 
//以下三种方法都是初始化hash桶的头结点 
#define HLIST_HEAD_INIT { .first = NULL } 
#define HLIST_HEAD(name) struct hlist_head name = { .first = NULL } 
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL) 
 
//初始化hash桶的普通结点 
static inline void INIT_HLIST_NODE(struct hlist_node *h) 
{ 
    h->next = NULL; 
    h->pprev = NULL; 
} 
 
//判断一个结点是否已经存在于hash桶中 
static inline int hlist_unhashed(const struct hlist_node *h) 
{ 
     return !h->pprev; 
} 
 
//判断一个hash桶是否为空 
static inline int hlist_empty(const struct hlist_head *h) 
{ 
     return !h->first; 
} 
 
static inline void __hlist_del(struct hlist_node *n) 
{ 
    struct hlist_node *next = n->next;//获取指向待删除结点的下一个结点的指针 
    struct hlist_node **pprev = n->pprev;//保留待删除结点的pprev域 
    *pprev = next;//修改待删除结点的pprev域，逻辑上使待删除结点的前驱结点指向待删除结点的后继结点 
    if (next) 
        next->pprev = pprev;//设置待删除结点的下一个结点的pprev域，保持hlist的结构 
} 
 
static inline void hlist_del(struct hlist_node *n) 
{ 
    __hlist_del(n);//删除结点之后，需要将其next域和pprev域设置为无用值 
    n->next = (hlist_node*) LIST_POISON1; 
    n->pprev = (hlist_node**) LIST_POISON2; 
} 
 
static inline void hlist_del_init(struct hlist_node *n) 
{ 
 if (!hlist_unhashed(n)) 
 { 
    __hlist_del(n); 
    INIT_HLIST_NODE(n); 
 } 
} 
 
//将普通结点n插入到头结点h对应的hash桶的第一个结点的位置 
static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h) 
{ 
    struct hlist_node *first = h->first; 
    n->next = first; 
    if (first) 
    first->pprev = &n->next; 
    h->first = n; 
    n->pprev = &h->first; 
} 
 
/* next must be != NULL */ 
//在next结点之前插入结点n，即使next结点是hash桶中的第一个结点也可以 
static inline void hlist_add_before(struct hlist_node *n, struct hlist_node *next) 
{ 
    n->pprev = next->pprev; 
    n->next = next; 
    next->pprev = &n->next; 
    *(n->pprev) = n; 
} 
 
//在结点n之后插入结点next 
static inline void hlist_add_after(struct hlist_node *n, struct hlist_node *next) 
{ 
    next->next = n->next; 
    n->next = next; 
    next->pprev = &n->next; 
    
    if(next->next) 
        next->next->pprev = &next->next; 
} 
 
/* 
 * Move a list from one list head to another. Fixup the pprev 
 * reference of the first entry if it exists. 
 */ 
static inline void hlist_move_list(struct hlist_head *old, struct hlist_head *new) 
{ 
    new->first = old->first; 
    if (new->first) 
    new->first->pprev = &new->first; 
    old->first = NULL; 
} 

unsigned long hash_long(unsigned long val, unsigned int bits)
{
    unsigned long hash = val * 0x9e370001UL;
    return hash >> (32 - bits);
}

//通过一个结构体内部一个成员的地址获取结构体的首地址 
#define hlist_entry(ptr, type, member) container_of(ptr,type,member) 
 
#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos && ({ prefetch(pos->next); 1; }); \
         pos = pos->next)
 
#define hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->next; 1; }); \
         pos = n)
  /** 
 * hlist_for_each_entry	- iterate over list of given type 
 * @tpos:	the type * to use as a loop cursor. 
 * @pos:	the &struct hlist_node to use as a loop cursor. 
 * @head:	the head for your list. 
 * @member:	the name of the hlist_node within the struct. 
 */ 
#define hlist_for_each_entry(tpos, pos, head, member) \
    for (pos = (head)->first; \
         pos && ({ prefetch(pos->next); 1;}) && \
            ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->next)
 /** 
 * hlist_for_each_entry_continue - iterate over a hlist continuing after current point 
 * @tpos:	the type * to use as a loop cursor. 
 * @pos:	the &struct hlist_node to use as a loop cursor. 
 * @member:	the name of the hlist_node within the struct. 
 */ 
#define hlist_for_each_entry_continue(tpos, pos, member) \
    for (pos = (pos)->next; \
         pos && ({ prefetch(pos->next); 1;}) && \
            ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->next)
/** 
 * hlist_for_each_entry_from - iterate over a hlist continuing from current point 
 * @tpos:	the type * to use as a loop cursor. 
 * @pos:	the &struct hlist_node to use as a loop cursor. 
 * @member:	the name of the hlist_node within the struct. 
 */ 
#define hlist_for_each_entry_from(tpos, pos, member) \
    for (; pos && ({ prefetch(pos->next); 1;}) && \
            ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->next)
/** 
 * hlist_for_each_entry_safe - iterate over list of given type safe against removal of list entry 
 * @tpos:	the type * to use as a loop cursor. 
 * @pos:	the &struct hlist_node to use as a loop cursor. 
 * @n:	 another &struct hlist_node to use as temporary storage 
 * @head:	the head for your list. 
 * @member:	the name of the hlist_node within the struct. 
 */ 
#define hlist_for_each_entry_safe(tpos, pos, n, head, member) \
    for (pos = (head)->first; \
         pos && ({ n = pos->next; 1; }) && \
            ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = n)

#endif