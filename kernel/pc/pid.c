#include <kos/pc/sched.h>

#define pid_hashfn( x ) hash_long( (unsigned long)x, pidhash_shift )

static pidmap_t pidmap_array[ 100 ];
static struct hlist_head *pid_hash;
static int pidhash_shift = 11;

pidmap_t pid_map;
int last_pid;

void pidmap_init()
{
	last_pid = -1;
	pid_map.nr_free = PID_MAX_DEFAULT;
	for ( int i = 0; i < PAGE_SIZE; i++ )
	{
		pid_map.page[ i ] = 0;
	}
}

int find_next_zero_bit( void *addr, int size, int offset )
{
	unsigned long *x;
	unsigned long mask;
	while ( offset < size )
	{
		x = ( (unsigned long *)addr ) + ( offset >> ( sizeof( unsigned long ) + 1 ) );
		mask = 1UL << ( offset & ( sizeof( unsigned long ) * BITS_PER_BYTE - 1 ) );
		if ( ( ~( *x ) & mask ) )
		{
			break;
		}
		++offset;
	}
	return offset;
}

int test_and_set_bit( int offset, void *addr )
{
	unsigned long mask = 1UL << ( offset & ( sizeof( unsigned long ) * BITS_PER_BYTE - 1 ) );
	unsigned long *p = ( (unsigned long *)addr ) + ( offset >> ( sizeof( unsigned long ) + 1 ) );
	unsigned long old = *p;
	*p = old | mask;
	return ( old & mask ) != 0;
}

int alloc_pidmap()
{
	int pid = last_pid + 1;
	int offset = pid & BITS_PER_PAGE_MASK;
	if ( !pid_map.nr_free )
	{
		return -1;
	}

	offset = find_next_zero_bit( &pid_map.page, BITS_PER_PAGE, offset );
	if ( BITS_PER_PAGE != offset && !test_and_set_bit( offset, &pid_map.page ) )
	{
		pid_map.nr_free--;
		last_pid = offset;
		return last_pid;
	}
	return -1;
}

void clear_bit( int offset, void *addr )
{
	unsigned long mask = 1UL << ( offset & ( sizeof( unsigned long ) * BITS_PER_BYTE - 1 ) );	  //取offset的后31位数据,并左移
	unsigned long *p = ( (unsigned long *)addr ) + ( offset >> ( sizeof( unsigned long ) + 1 ) );  //+优先级高于>>
	unsigned long old = *p;
	*p = old & ~mask;
}

void free_pidmap( int pid )
{
	int offset = pid & BITS_PER_PAGE_MASK;
	pid_map.nr_free++;
	clear_bit( offset, &pid_map.page );
}

void pidhash_initial()
{
	int pidhash_size = 1 << pidhash_shift;  //2048
	pid_hash = alloc_bootmem( pidhash_size * sizeof( *( pid_hash ) ) );
	if ( !pid_hash )
		kernel_printf( "Could not alloc pidhash!\n" );
	for ( int i = 0; i < pidhash_size; i++ )
		INIT_HLIST_HEAD( &pid_hash[ i ] );
}

struct pid *find_pid( int nr )
{
	struct hlist_node *elem;
	struct pid *pid;

	hlist_for_each_entry( pid, elem, &pid_hash[ pid_hashfn( nr ) ], pid_chain )
	{
		if ( pid->nr == nr )
			return pid;
	}
	return NULL;
}

struct task_struct *find_task_by_pid( int nr )
{
	struct pid *pid;
	pid = find_pid( nr );
	if ( pid == NULL )
	{
		return NULL;
	}
	return pid_task( &pid->pid_list );
}

void attach_pid( struct task_struct *task, int nr )
{
	struct pid *pid, *task_pid;
	task_pid = &( task->pids );

	pid = find_pid( nr );
	if ( pid == NULL )
	{
		hlist_add_head( &( task_pid->pid_chain ), &pid_hash[ pid_hashfn( nr ) ] );
		INIT_LIST_HEAD( &task_pid->pid_list );
	}
	else
	{
		INIT_HLIST_NODE( &task_pid->pid_chain );
		list_add_tail( &task_pid->pid_list, &pid->pid_list );
	}
	task_pid->nr = nr;
}

void detach_pid( struct task_struct *task )
{
	struct pid *pid, *pid_next;
	int nr = 0;

	pid = &task->pids;
	if ( !hlist_unhashed( &pid->pid_chain ) )
	{
		hlist_del( &pid->pid_chain );

		if ( list_empty( &pid->pid_list ) )
			nr = pid->nr;
		else
		{
			pid_next = list_entry( pid->pid_list.next,
								   struct pid, pid_list );
			hlist_add_head( &pid_next->pid_chain,
							&pid_hash[ pid_hashfn( pid_next->nr ) ] );
		}
	}
	if ( !nr )
	{
		return;
	}

	if ( find_pid( nr ) == NULL )
	{
		return;
	}

	free_pidmap( nr );
}