#include <kos/sched.h>
#include <arch/mips32/intr.h>

static void init_cfs_rq( struct cfs_rq *cfs_rq )
{
	cfs_rq->tasks_timeline = RB_ROOT;
	cfs_rq->min_vruntime = (unsigned long long)( -( 1LL << 20 ) );
}

static void set_load_weight( struct task_struct *p )
{
	if ( p->policy == SCHED_IDLE )
	{
		p->se.load.weight = WEIGHT_IDLEPRIO;
		p->se.load.inv_weight = WMULT_IDLEPRIO;
		return;
	}
	p->se.load.weight = prio_to_weight[ p->prioiry - MAX_RT_PRIO ];
	p->se.load.inv_weight = prio_to_wmult[ p->prioiry - MAX_RT_PRIO ];
}

static int need_resched( struct task_struct *p )
{
	if ( unlikely( p->THREAD_FLAG == TIF_NEED_RESCHED ) )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static void clear_tsk_need_resched( struct task_struct *p )
{
	p->THREAD_FLAG = 0;
	//ThREAD_FLAG = 0;
}

static inline struct task_struct *pick_next_task( struct cfs_rq *cfs_rq, struct task_struct *prev )
{
	struct task_struct *p;
	p = pick_next_task_fair( cfs_rq );
	if ( likely( p ) )
	{
		return p;
	}
	else
	{
		kernel_printf( "error in pick_next_task\n" );
	}
}

static void init_cfs_rq( struct cfs_rq *cfs_rq )
{
	cfs_rq->tasks_timeline = RB_ROOT;
	cfs_rq->min_vruntime = (unsigned long long)( -( 1LL << 20 ) );
	cfs_rq->exec_clock = 0;
	cfs_rq->nr_running = 0;
	cfs_rq->clock = 1;
	cfs_rq->prev_clock_raw = 0;
	cfs_rq->clock_max_delta = 0;
	cfs_rq->current_task = &init_task;
}

static void update_cfs_clock( struct cfs_rq *cfs_rq )
{
	unsigned long long prev_clock = cfs_rq->prev_clock_raw;
	unsigned long long now = timeCount * CLOCK_INTERRUPTER_TICK;
	signed long long delta = now - prev_clock;
	unsigned long long clock = cfs_rq->clock;

	if ( unlikely( delta > cfs_rq->clock_max_delta ) )
		cfs_rq->clock_max_delta = delta;
	clock += delta;

	cfs_rq->prev_clock_raw = now;
	cfs_rq->clock = clock;
}
void init_idle( struct task_struct *idle )
{
	//
	struct cfs_rq *cfs_rq = get_cfs();
	cfs_rq->idle = idle;
}
void sched_init()
{
	set_load_weight( &init_task );
	init_idle( &init_task );
	init_cfs_rq( get_cfs() );
	pidhash_initial();
	pidmap_init();

	register_interrupt_handler( 7, scheduler_tick );
	asm volatile(
	  "li $v0, 1000000\n\t"
	  "mtc0 $v0, $11\n\t"
	  "mtc0 $zero, $9" );
}

void scheduler_tick( unsigned int status, unsigned int cause, struct reg_context *pt_context )
{
	timeCount++;
	struct cfs_rq *cfs_rq = get_cfs();
	struct task_struct *curr = cfs_rq->current_task;

	update_cfs_clock( cfs_rq );
	//time update

	task_tick_fair( cfs_rq, curr );

	scheduler( pt_context );
}

void scheduler( struct reg_context *pt_context )
{
	struct task_struct *prev, *next;
	struct cfs_rq *cfs_rq;
	cfs_rq = get_cfs();
	disable_interrupts();
	do
	{
		prev = cfs_rq->current_task;
		update_cfs_clock( cfs_rq );
		clear_tsk_need_resched( prev );

		if ( unlikely( !cfs_rq->nr_running ) )
		{
			kernel_printf( "I am idle\n" );
			// if only one task idle
			break;
		}

		put_prev_task_fair( cfs_rq, prev );
		next = pick_next_task( cfs_rq, prev );

		if ( likely( prev != next ) )
		{
			copy_context( pt_context, &( cfs_rq->current_task->context ) );

			cfs_rq->current_task = next;
			cfs_rq->curr = &next->se;

			copy_context( pt_context, &( cfs_rq->current_task->context ) );
			//context_switch!!
		}
	} while ( need_resched( next ) );
	enable_interrupts();
}

void sched_fork( struct task_struct *p )
{
	p->se.exec_start = 0;
	p->se.sum_exec_runtime = 0;
	p->se.prev_sum_exec_runtime = 0;
	p->se.vruntime = 0;

	p->se.exec_max = 0;
	p->se.wait_start = 0;
	p->se.wait_max = 0;

	p->prioiry = NICE_TO_PRIO( 0 );
	set_load_weight( p );

	p->se.on_rq = 0;
	p->state = TASK_RUNNING;
}

void wake_up_new_task( struct task_struct *p )
{
	struct cfs_rq *cfs_rq = get_cfs();
	update_cfs_clock( cfs_rq );

	task_new_fair( cfs_rq, p );

	cfs_rq->nr_running++;
	cfs_rq->load.weight += p->se.load.weight;

	check_preempt_wakeup( cfs_rq, p );
}

int task_fork( char *name, void ( *entry )( unsigned int argc, void *args ), unsigned int argc, void *args, pid_t *returnpid )
{
	struct task_struct *p;
	struct cfs_rq *cfs_rq = get_cfs();
	unsigned int init_gp;

	pid_t newpid = alloc_pidmap();
	if ( newpid == -1 )
	{
		free_pidmap( newpid );
		kernel_printf( "pid allocated failed\n" );
		return 0;
	}

	p = (struct task_struct *)kmalloc( sizeof( struct task_struct ) );
	if ( p == 0 )
	{
		kernel_printf( "task_struct allocated failed\n" );
		return 0;
	}

	p->pid = newpid;
	p->parent = cfs_rq->current_task->pid;
	p->policy = SCHED_NORMAL;
	p->prioiry = cfs_rq->current_task->prioiry;
	p->THREAD_FLAG = 0;

	sched_fork( p );

	kernel_strcpy( p->name, name );

	kernel_memset( &( p->context ), 0, sizeof( struct reg_context ) );
	p->context.epc = (unsigned int)entry;
	asm volatile( "la %0, _gp\n\t"
				  : "=r"( init_gp ) );
	p->context.gp = init_gp;
	p->context.a0 = argc;
	p->context.a1 = (unsigned int)args;

	if ( returnpid != 0 )
	{
		*returnpid = newpid;
	}

	//
	attach_pid( p, newpid );

	wake_up_new_task( p );
	return 1;
}

int exec( unsigned int argc, void *args, int is_wait )
{
	pid_t newpid;
	int result = task_fork( args, (void *)execproc, argc, args, &newpid, 0 );
	if ( result != 0 )
	{
		kernel_printf( "exec: task created failed!\n" );
		return 0;
	}

	if ( is_wait )
	{
		//waitpid( newpid );
	}
	return 0;
}

int execproc( unsigned int argc, void *args )
{
	int count1 = 0, count2 = 0;
	struct cfs_rq *cfs_rq = get_cfs();
	struct task_struct *current = cfs_rq->current_task;

	kernel_printf( "\n********execproc start*********\n" );
	kernel_printf( "current_task: %d\n", current->pid );
	kernel_printf( "**********execproc end************\n" );

	if ( kernel_strcmp( args, "loop" ) == 0 )
	{
		while ( 1 )
			;
	}
	while ( 1 )
	{
		count1++;
		if ( count2 == 2 )
			break;
		if ( count1 == 10000000 )
		{
			kernel_printf( "\ncurrent_task: %d \n", current->pid );
			count1 = 0;
			count2++;
		}
	}

	kernel_printf( "\ncurrent_task: %d  End.......\n", current->pid );

	do_exit();

	return 1;
}

static int try_to_wake_up( struct task_struct *p )
{
	int sucess = 0;
	if ( p->se.on_rq )
	{
		p->state = TASK_RUNNING;
	}
out_running:
	p->state = TASK_RUNNING;
out:
	return success;
}

void do_exit()
{
	struct cfs_rq *cfs_rq = get_cfs();
	struct task_struct *current = cfs_rq->current_task, *next;

	if ( current->pid == 0 || current->pid == 1 )
	{
		kernel_printf( "You cannot exit this process\n" );
		while ( 1 )
			;
	}

	asm volatile(
	  "mfc0  $t0, $12\n\t"
	  "ori   $t0, $t0, 0x02\n\t"
	  "mtc0  $t0, $12\n\t"
	  "nop\n\t"
	  "nop\n\t" );

	current->state = TASK_DEAD;

	struct task_struct *parent = find_task_by_pid( current->parent );
	try_to_wake_up( parent );
}

static void set_priority( struct task_struct *p, long prioiry )
{
	struct cfs_rq *cfs_rq = get_cfs();
	update_cfs_clock( cfs_rq );

	int on_rq = p->se.on_rq;
	if ( on_rq )
	{
		dequeu_task_fair( cfs_rq, p, 0 );
		p->se.on_rq = 0;
		cfs_rq->load.weight -= p->se.load.weight;
	}

	int old_priority = p->prioiry;
	p->prioiry = prioiry;
	set_load_weight( p );
	int delta = prioiry - old_priority;
	if ( on_rq )
	{
		enqueue_task_fair( cfs_rq, p, 0 );
		cfs_rq->load.weight += p->se.load.weight;
		p->se.on_rq = 1;
		if ( delta < 0 || ( delta > 0 && cfs_rq->current_task == p ) )
		{
			cfs_rq->current_task->THREAD_FLAG = TIF_NEED_RESCHED;
		}
	}
}

void sys_prioiry( int increment )
{
	long prioiry;
	struct cfs_rq *cfs_rq = get_cfs();
	struct task_struct *current = cfs_rq->current_task;
	if ( increment < -40 )
		increment = -40;
	if ( increment > 40 )
		increment = 40;

	prioiry = current->prioiry + increment;
	if ( prioiry < 100 )
	{
		prioiry = 100;
	}
	if ( prioiry > 140 )
	{
		prioiry = 140;
	}

	set_priority( current, prioiry );
}