#include <kos/sched.h>

#define pid_hashfn( x ) hash_long( (unsigned long)x, pidhash_shift )
static struct hlist_head *pid_hash;
static int pidhash_shift = 11;

int pid_max = PID_MAX_DEFAULT;

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

task_t *find_task_by_pid( int nr )
{
	struct pid *pid;
	pid = find_pid( nr );
	if ( pid == NULL )
	{
		return NULL;
	}
	return pid_task( &pid->pid_list, type );
}

void attach_pid( task_t *task, int nr )
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

void detach_pid( task_t *task )
{
}