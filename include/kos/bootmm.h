#ifndef _ZJUNIX_BOOTMM_H
#define _ZJUNIX_BOOTMM_H

#include <kos/mm/page.h>

#define MAX_DATA 10

extern unsigned char __end[];

enum mm_usage
{
	_MM_KERNEL,
	_MM_MMMAP,
	_MM_VGABUFF,
	_MM_PDTABLE,
	_MM_PTABLE,
	_MM_DYNAMIC,
	_MM_RESERVED,
	_MM_COUNT
};

/* record mm's data sections */
struct bootmm_info {
    unsigned int start;
    unsigned int end;
    unsigned int type;
};

typedef struct bootmm_info * p_mminfo;


struct bootmm
{
	unsigned int phymm;	// the actual physical memory
	unsigned int max_pfn;  // record the max page number
	unsigned int boot_start; //保存了系统中第一个页的编号
	unsigned char *memmap;  // 指向存储分配位图的内存区的指针,在连接期间自动的地插入到内核映像之中
	unsigned char *memmap_end;
	unsigned int last_success; //指定位图上一次成功分配内存的位置，新的分配将由此开始
	unsigned int cnt_infos;  // current number of infos stored in bootmm now
	struct bootmm_info info[ MAX_DATA ];
};

typedef struct bootmm * p_bootmm;

extern struct bootmm bmm;

extern unsigned int firstusercode_start;
extern unsigned int firstusercode_len;

extern unsigned int get_phymm_size();

extern void set_mminfo( p_mminfo info, unsigned int start, unsigned int end, unsigned int type );

extern unsigned int insert_mminfo( p_bootmm mm, unsigned int start, unsigned int end, unsigned int type );

extern unsigned int split_mminfo( p_bootmm mm, unsigned int index, unsigned int split_start );

extern void remove_mminfo( p_bootmm mm, unsigned int index );

extern void init_bootmm();

extern void set_maps( unsigned int s_pfn, unsigned int cnt, unsigned char value );

extern unsigned char *find_pages( unsigned int page_cnt, unsigned int s_pfn, unsigned int e_pfn, unsigned int align_pfn );

extern unsigned char *bootmm_alloc_pages( unsigned int size, unsigned int type, unsigned int align );

extern unsigned char *alloc_bootmem( unsigned int size );

extern void bootmap_info( unsigned char *msg );

#endif
