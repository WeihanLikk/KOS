#ifndef _PID_H
#define _PID_H

#include <kos/hlist.h>
#include <kos/list.h>

#define PID_MAX_DEFAULT 0x8000
#define pid_task( elem ) \
	list_entry( elem, struct task_struct, pids.pid_list )
#define PID_MAX_DEFAULT 0x8000 /* max pid, equal to 2^15=32768 */

// #ifndef PAGE_SHIFT
// #define PAGE_SHIFT 12
// #endif

// #ifndef PAGE_SIZE
// #define PAGE_SIZE ( 1UL << PAGE_SHIFT ) /* page size = 2^12 = 4K */
// #endif

#define PID_NUM 256
#define PID_BYTES ( ( PID_NUM + 7 ) >> 3 )

#define BITS_PER_BYTE 8
//4k*8 32768
#define BITS_PER_PAGE ( PID_BYTES * BITS_PER_BYTE )
//7fff
//0111 1111 1111 1111
#define BITS_PER_PAGE_MASK ( BITS_PER_PAGE - 1 )
#define RESERVED_PIDS 10
#define pid_hashfn( nr ) hash_long( (unsigned long)nr, pidhash_shift )
typedef int pid_t;

typedef struct pidmap
{
	int nr_free;
	char page[ PID_BYTES ];
} pidmap_t;

struct pid
{
	struct list_head pid_list;	//指回 pid_link 的 node
	int nr;						  //PID
	struct hlist_node pid_chain;  //pid hash 散列表结点
};

void pidhash_initial();
void pidmap_init();
int alloc_pidmap();
void free_pidmap( int pid );
struct pid *find_pid( int nr );
struct task_struct *find_task_by_pid( int nr );
void attach_pid( struct task_struct *task, int nr );
void detach_pid( struct task_struct *task );

#endif