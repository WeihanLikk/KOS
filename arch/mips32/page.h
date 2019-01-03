#ifndef _PAGE__H
#define _PAGE__H

void init_pgtable();

typedef struct {
    unsigned int Mask : 12; //配置映射页的大小 12 OR 16?
    unsigned int reserved0 : 20;
} __PageMask;

typedef struct {
    unsigned int ASID : 8; //地址空间标识符
    unsigned int reserved : 5;
    unsigned int VPN2 : 19; //virtual page number
} __EntryHi;

typedef struct {
    unsigned int G : 1; //当tlb表项G位置位时，tlb项将仅仅在VPN上匹配，而不管tlb项的ASID域是否匹配EntryHi中的值
    unsigned int V : 1; //有效位，为0则对该项的任何操作都导致异常
    unsigned int D : 1; //写允许位。置位位1允许写入，0导致该地址转换的写入操作发生自陷
    unsigned int C : 3; //一个三位的域，用来设置“高速缓冲算法”
    unsigned int PFN : 24; //page frame number
    unsigned int reserved : 2;
} __EntryLo;

typedef struct {
    __EntryLo EntryLo0; 
    __EntryLo EntryLo1;
    __EntryHi EntryHi; //VPN2(13-31), ASID(0-7)
    __PageMask PageMask; //配置映射页的大小
} PageTableEntry;

typedef PageTableEntry pgd_t;
typedef PageTableEntry pte_t;

PageTableEntry set_pgtable_entry(unsigned int pgd_v);
int find_in_pgtable(unsigned int vaddr);


#endif
