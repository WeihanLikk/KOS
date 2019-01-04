#include <kos/pc/sched.h>
//#include <kos/vm/vmm.h>
#include <driver/vga.h>
#include <intr.h>
#include <kos/log.h>
#include <usr/ps.h>

struct list_head wait;
struct list_head exited;

struct task_struct init_task = INIT_TASK( init_task );
struct cfs_rq my_cfs_rq = INIT_CFS_RQ( my_cfs_rq );

static void copy_context( struct reg_context *src, struct reg_context *dest )
{
	dest->epc = src->epc;
	dest->at = src->at;
	dest->v0 = src->v0;
	dest->v1 = src->v1;
	dest->a0 = src->a0;
	dest->a1 = src->a1;
	dest->a2 = src->a2;
	dest->a3 = src->a3;
	dest->t0 = src->t0;
	dest->t1 = src->t1;
	dest->t2 = src->t2;
	dest->t3 = src->t3;
	dest->t4 = src->t4;
	dest->t5 = src->t5;
	dest->t6 = src->t6;
	dest->t7 = src->t7;
	dest->s0 = src->s0;
	dest->s1 = src->s1;
	dest->s2 = src->s2;
	dest->s3 = src->s3;
	dest->s4 = src->s4;
	dest->s5 = src->s5;
	dest->s6 = src->s6;
	dest->s7 = src->s7;
	dest->t8 = src->t8;
	dest->t9 = src->t9;
	dest->hi = src->hi;
	dest->lo = src->lo;
	dest->gp = src->gp;
	dest->sp = src->sp;
	dest->fp = src->fp;
	dest->ra = src->ra;
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

static int need_resched()
{
	struct task_struct *p = my_cfs_rq.current_task;
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

// static void init_cfs_rq( struct cfs_rq *cfs_rq )
// {
// 	cfs_rq->tasks_timeline = RB_ROOT;
// 	cfs_rq->min_vruntime = (unsigned long long)( -( 1LL << 20 ) );
// 	cfs_rq->exec_clock = 0;
// 	cfs_rq->nr_running = 0;
// 	cfs_rq->clock = 1;
// 	cfs_rq->prev_clock_raw = 0;
// 	cfs_rq->clock_max_delta = 0;
// 	cfs_rq->current_task = &init_task;
// }

static void update_cfs_clock( struct cfs_rq *cfs_rq )
{
	unsigned long long prev_clock = cfs_rq->prev_clock_raw;
	unsigned int ticks_high, ticks_low;
	asm volatile(
	  "mfc0 %0, $9, 6\n\t"
	  "mfc0 %1, $9, 7\n\t"
	  : "=r"( ticks_low ), "=r"( ticks_high ) );

	unsigned long long now = ticks_high;
	now << 32;
	now += ticks_low;

	kernel_printf( "Error in cfs_clock1\n" );
	// unsigned long long now = timeCount * CLOCK_INTERRUPTER_TICK;
	signed long long delta = now - prev_clock;
	unsigned long long clock = cfs_rq->clock;

	kernel_printf( "Error in cfs_clock2\n" );

	if ( unlikely( delta > cfs_rq->clock_max_delta ) )
		cfs_rq->clock_max_delta = delta;
	clock += delta;

	kernel_printf( "Error in cfs_clock3\n" );
	cfs_rq->prev_clock_raw = now;
	cfs_rq->clock = clock;
}
void init_idle( struct task_struct *idle )
{
	//
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	cfs_rq->idle = idle;
	cfs_rq->idle->pid = alloc_pidmap();
	if ( cfs_rq->idle->pid != 0 )
	{
		//log( LOG_OK, "Here ka zhu le." );
		kernel_printf( "idle pid error\n" );
		kernel_printf( "The idle pid: %d\n", cfs_rq->idle->pid );
	}
}

static void create_shell_process()
{
	struct task_struct *p;
	struct cfs_rq *cfs_rq = &my_cfs_rq;
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
	p->parent = cfs_rq->idle->pid;
	p->policy = SCHED_NORMAL;
	p->prioiry = cfs_rq->idle->prioiry;
	p->THREAD_FLAG = 0;

	sched_fork( p );

	kernel_strcpy( p->name, "shell" );

	kernel_memset( &( p->context ), 0, sizeof( struct reg_context ) );

	void( *entry ) = (void *)ps;
	p->context.epc = (unsigned int)entry;
	asm volatile( "la %0, _gp\n\t"
				  : "=r"( init_gp ) );
	p->context.gp = init_gp;
	p->context.a0 = 0;
	p->context.a1 = 0;
	//
	//attach_pid( p, newpid );

	cfs_rq->current_task = p;
	cfs_rq->curr = &p->se;
	wake_up_new_task( p );

	kernel_printf( "kernel shell created\n" );
}

void sched_init()
{
	//pidhash_initial();
	pidmap_init();

	set_load_weight( &init_task );
	init_idle( &init_task );
	my_cfs_rq.current_task = &init_task;

	INIT_LIST_HEAD( &exited );
	INIT_LIST_HEAD( &wait );

	register_interrupt_handler( 7, scheduler_tick );
	asm volatile(
	  "li $v0, 1000000\n\t"
	  "mtc0 $v0, $11\n\t"
	  "mtc0 $zero, $9" );

	create_shell_process();
}

static void scheduler( struct reg_context *pt_context )
{
	struct task_struct *prev, *next;
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	//disable_interrupts();
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

			copy_context( &( cfs_rq->current_task->context ), pt_context );
			//context_switch!!
		}
	} while ( need_resched() );
	//enable_interrupts();
}

void scheduler_tick( unsigned int status, unsigned int cause, struct reg_context *pt_context )
{
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	struct task_struct *curr = cfs_rq->current_task;

	update_cfs_clock( cfs_rq );
	//time update

	task_tick_fair( cfs_rq, curr );

	scheduler( pt_context );
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
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	update_cfs_clock( cfs_rq );

	task_new_fair( cfs_rq, p );

	cfs_rq->nr_running++;
	cfs_rq->load.weight += p->se.load.weight;

	check_preempt_wakeup( cfs_rq, p );
}

int task_fork( char *name, void ( *entry )( unsigned int argc, void *args ), unsigned int argc, void *args, pid_t *returnpid, int is_vm )
{
	struct task_struct *p;
	struct cfs_rq *cfs_rq = &my_cfs_rq;
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

	if ( is_vm )
	{
		p->mm = mm_create();
	}
	else
	{
		p->mm = 0;
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
	//attach_pid( p, newpid );

	kernel_printf( "third down\n" );
	wake_up_new_task( p );

	//kernel_printf( "forth down\n" );
	return 1;
}

static int execproc( unsigned int argc, void *args )
{
	int count1 = 0, count2 = 0;
	struct cfs_rq *cfs_rq = &my_cfs_rq;
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

	kernel_printf( "Error: execproc, do_exit\n" );
	while ( 1 )
		;
	return 1;
}

int execk( unsigned int argc, void *args, int is_wait )
{
	pid_t newpid;
	int result = task_fork( args, (void *)execproc, argc, args, &newpid, 1 );
	if ( result != 0 )
	{
		kernel_printf( "exec: task created failed\n" );
		return 0;
	}

	if ( is_wait )
	{
		//waitpid( newpid );
	}
	return 0;
}

static int try_to_wake_up( struct task_struct *p )
{
	int success = 0;
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	if ( p->se.on_rq )
	{
		p->state = TASK_RUNNING;
		goto out_running;
	}
	update_cfs_clock( cfs_rq );
	enqueue_task_fair( cfs_rq, p, 1 );
	p->se.on_rq = 1;
	cfs_rq->nr_running++;
	cfs_rq->load.weight += p->se.load.weight;
	check_preempt_wakeup( cfs_rq, p );
	success = 1;

out_running:
	p->state = TASK_RUNNING;

	return success;
}

static void add_wait( struct task_struct *task )
{
	list_add_tail( &( task->node ), &wait );
}

static void add_exited( struct task_struct *task )
{
	list_add_tail( &( task->node ), &exited );
}

static void remove_wait( struct task_struct *p )
{
	list_del( &( p->node ) );
	INIT_LIST_HEAD( &( p->node ) );
}

void do_exit()
{
	struct cfs_rq *cfs_rq = &my_cfs_rq;
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
	int res = try_to_wake_up( parent );
	if ( res == 0 )
	{
		kernel_printf( "The parent is alreadly in cfs_rq\n" );
	}

	if ( current->se.on_rq )
	{
		dequeu_task_fair( cfs_rq, current, 0 );
	}
	free_pidmap( current->pid );
	detach_pid( current );
	add_exited( current );
	// prev = cfs_rq->current_task;
	update_cfs_clock( cfs_rq );

	//put_prev_task_fair( cfs_rq, current );
	next = pick_next_task( cfs_rq, current );

	if ( likely( current != next ) )
	{
		// copy_context( pt_context, &( cfs_rq->current_task->context ) );

		cfs_rq->current_task = next;
		cfs_rq->curr = &next->se;
		switch_ex( &( cfs_rq->current_task->context ) );
		// copy_context( &( cfs_rq->current_task->context ), pt_context );
		//context_switch!!
	}

	kernel_printf( "Error: pc_exit\n" );
}

//1-success, 0-unsuccess
int pc_kill( pid_t pid )
{
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	if ( pid == 0 || pid == 1 || cfs_rq->current_task->pid )
	{
		kernel_printf( "Cannot kill this\n" );
		return 0;
	}

	disable_interrupts();

	if ( !find_pid( pid ) )
	{
		kernel_printf( "pid not found\n" );
		enable_interrupts();
		return 0;
	}

	struct task_struct *p = find_task_by_pid( pid );
	p->state = TASK_DEAD;
	if ( p->se.on_rq )
	{
		dequeu_task_fair( cfs_rq, p, 0 );
	}
	else
	{
		kernel_printf( "Error in kill, why this process is not on cfs_rq?\n" );
	}
	add_exited( p );
	free_pidmap( pid );
	detach_pid( p );

	enable_interrupts();
	return 1;
}

void waitpid( pid_t pid )
{
	disable_interrupts();
	struct task_struct *p = find_task_by_pid( pid );
	if ( !p->se.on_rq )
	{
		kernel_printf( "The child process is not onrq\n" );
		enable_interrupts();
		return;
	}
	enable_interrupts();

	asm volatile(
	  "mfc0  $t0, $12\n\t"
	  "ori   $t0, $t0, 0x02\n\t"
	  "mtc0  $t0, $12\n\t"
	  "nop\n\t"
	  "nop\n\t" );

	struct cfs_rq *cfs_rq = &my_cfs_rq;
	struct task_struct *current = cfs_rq->current_task;

	current->state = TASK_WAITING;
	if ( current->se.on_rq )
	{
		dequeu_task_fair( cfs_rq, current, 0 );
	}
	add_wait( current );

	update_cfs_clock( cfs_rq );
	struct task_struct *next = pick_next_task( cfs_rq, current );

	if ( likely( current != next ) )
	{
		cfs_rq->current_task = next;
		cfs_rq->curr = &next->se;
		switch_wa( &( next->context ), &( current->context ) );
	}
	else
	{
		kernel_printf( "Error if comes to here in waitpid\n" );
	}
}

static void set_priority( struct task_struct *p, long prioiry )
{
	struct cfs_rq *cfs_rq = &my_cfs_rq;
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
	struct cfs_rq *cfs_rq = &my_cfs_rq;
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