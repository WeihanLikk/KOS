#include <kos/pc/sched.h>
#include <kos/vm/vmm.h>
#include <driver/vga.h>
#include <intr.h>
#include <kos/log.h>
#include <kos/slab.h>
#include <kos/ps.h>
#include <arch.h>

struct list_head wait;
struct list_head exited;
struct list_head tasks;

struct cfs_rq my_cfs_rq;
struct task_struct *idle_task;
struct task_struct *current_task = 0;
int countx = 0;
unsigned long total = 0;

struct task_struct *p;

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
	//kernel_printf( "check the thread: %x\n", p->THREAD_FLAG );
	//return 0;
	if ( p->THREAD_FLAG == TIF_NEED_RESCHED )
	{
		p->THREAD_FLAG = 0;
		//kernel_printf( "???? be here111\n" );
		return 1;
	}
	else
	{
		//kernel_printf( "Must be here222\n" );
		return 0;
	}
}

static void add_wait( struct task_struct *task )
{
	list_add_tail( &( task->node ), &wait );
}

static void add_exited( struct task_struct *task )
{
	list_add_tail( &( task->node ), &exited );
}

static void add_task( struct task_struct *task )
{
	list_add_tail( &( task->node ), &tasks );
}

static void remove( struct task_struct *task )
{
	list_del( &( task->node ) );
	INIT_LIST_HEAD( &( task->node ) );
}

static void clear_tsk_need_resched( struct task_struct *p )
{
	p->THREAD_FLAG = 0;
	//kernel_printf( "check the falg in clear: %x\n", p->THREAD_FLAG );
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

static void sched_fork( struct task_struct *p )
{
	p->se.exec_start = 0;
	p->se.sum_exec_runtime = 0;
	p->se.prev_sum_exec_runtime = 0;
	p->se.vruntime = 0;

	p->se.exec_max = 0;
	p->se.wait_start = 0;
	p->se.wait_max = 0;

	//p->prioiry = NICE_TO_PRIO( 0 );
	set_load_weight( p );

	p->se.on_rq = 0;
	p->state = TASK_RUNNING;
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
	unsigned long prev_clock = cfs_rq->prev_clock_raw;
	unsigned int ticks_high, ticks_low;
	asm volatile(
	  "mfc0 %0, $9, 6\n\t"
	  "mfc0 %1, $9, 7\n\t"
	  : "=r"( ticks_low ), "=r"( ticks_high ) );
	//kernel_printf( "here??\n" );
	unsigned long now;
	ticks_low = ( ticks_low >> 3 | ticks_high << 29 );
	ticks_high >>= 3;
	now = ( ticks_high % 125 );
	now = now * 10995 + ( now * 45421 ) % 125;
	now += ticks_low / 125;

	//now << 32;
	// now += ticks_low;

	//kernel_printf( "pass in cfs_clock1\n" );
	// // unsigned long long now = timeCount * CLOCK_INTERRUPTER_TICK;
	signed long delta = now - prev_clock;
	unsigned long clock = cfs_rq->clock;

	//kernel_printf( "pass in cfs_clock2\n" );

	if ( unlikely( delta > cfs_rq->clock_max_delta ) )
		cfs_rq->clock_max_delta = delta;
	clock += delta;

	//kernel_printf( "pass in cfs_clock3\n" );
	cfs_rq->prev_clock_raw = now;
	cfs_rq->clock = clock;
	//kernel_printf( "The clock is %x\n", cfs_rq->clock );
}

static void wake_up_new_task( struct task_struct *p )
{
	if ( p == 0 )
	{
		kernel_printf( "wake up new task\n" );
		while ( 1 )
			;
	}
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	update_cfs_clock( cfs_rq );
	//kernel_printf( "check the clock: %x\n", cfs_rq->clock );

	task_new_fair( cfs_rq, p );

	//cfs_rq->nr_running++;
	cfs_rq->load.weight += p->se.load.weight;

	check_preempt_wakeup( cfs_rq, p );
	if ( cfs_rq->current_task->THREAD_FLAG == TIF_NEED_RESCHED )
	{
		kernel_printf( "set the resched flag to current task in wake up\n" );
	}
}

int testtest()
{
	int countxx = 0;
	kernel_printf( "HERe !!!!!!!!!\n" );
	for ( int i = 0; i < 100000000000; i++ )
	{
		countxx++;
		if ( countxx == 100000000 )
		{
			kernel_printf( "testtest can do it\n" );
		}
	}
}

static void create_shell_process( char *name, void ( *entry )( unsigned int argc, void *args ), unsigned int argc, void *args, pid_t *returnpid, int is_vm )
{
	union thread_union *new;
	//struct task_struct *p;
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	unsigned int init_gp;

	pid_t newpid = alloc_pidmap();

	if ( newpid == -1 )
	{
		free_pidmap( newpid );
		kernel_printf( "pid allocated failed\n" );
		return;
	}

	new = (union thread_union *)kmalloc( sizeof( union thread_union ) );

	//kernel_printf( "Down here1\n" );

	if ( new == 0 )
	{
		kernel_printf( "task_struct allocated failed\n" );
		return;
	}

	new->task.pid = newpid;
	new->task.ASID = newpid;
	new->task.parent = cfs_rq->idle->pid;
	new->task.policy = SCHED_NORMAL;
	new->task.prioiry = NICE_TO_PRIO( 10 );
	new->task.THREAD_FLAG = 0;

	//kernel_memset( &( new->task.se ), 0, sizeof( struct sched_entity ) );
	sched_fork( &new->task );
	//p->se.exec_start = 0;
	//kernel_printf( "Down here2\n" );

	kernel_strcpy( new->task.name, "shell" );

	kernel_memset( &( new->task.context ), 0, sizeof( struct reg_context ) );

	//kernel_printf( "Down here3\n" );

	new->task.context.epc = (unsigned int)entry;

	new->task.context.sp = (unsigned int)new + KERNEL_STACK_SIZE;

	asm volatile( "la %0, _gp\n\t"
				  : "=r"( init_gp ) );
	new->task.context.gp = init_gp;
	new->task.context.a0 = argc;
	new->task.context.a1 = (unsigned int)args;

	attach_pid( &new->task, newpid );

	// kernel_printf( "Down here4\n" );
	// kernel_printf( "check the epc in create: %x asd\n", new->task.context.epc );
	// kernel_printf( "check the execstart in create: %x asd\n", new->task.se.exec_start );

	my_cfs_rq.current_task = &new->task;
	my_cfs_rq.curr = &new->task.se;
	add_task( &new->task );
	wake_up_new_task( &new->task );
	//kernel_printf( "kernel shell created\n" );
}

static void init_task( struct task_struct *p )
{
	idle_task = (struct task_struct *)( kernel_sp - KERNEL_STACK_SIZE );
	kernel_strcpy( idle_task->name, "idle" );
	idle_task->state = TASK_RUNNING_IDLE;
	idle_task->THREAD_FLAG = 0;
	idle_task->pid = alloc_pidmap();
	idle_task->ASID = idle_task->pid;
	idle_task->parent = 0;
	idle_task->policy = SCHED_NORMAL;
	idle_task->prioiry = 139;
	kernel_memset( &( idle_task->se ), 0, sizeof( struct sched_entity ) );
	set_load_weight( idle_task );
	attach_pid( idle_task, idle_task->pid );
}

static void init_cfs_rq( struct cfs_rq *rq )
{
	rq->tasks_timeline = RB_ROOT;
	rq->min_vruntime = (unsigned long)( -( 1LL << 20 ) );
	rq->exec_clock = 0;
	rq->nr_running = 0;
	rq->clock = 1;
	rq->prev_clock_raw = 0;
	rq->clock_max_delta = 0;
	rq->load.weight = 0;
	rq->rb_leftmost = NULL;
	rq->rb_load_balance_curr = NULL;
	kernel_memset( &( rq->load ), 0, sizeof( struct load_weight ) );
	rq->idle = idle_task;
	rq->current_task = current_task = idle_task;
	rq->curr = &idle_task->se;
}

void sched_init()
{
	pidhash_initial();
	pidmap_init();

	init_task( idle_task );
	init_cfs_rq( &my_cfs_rq );

	INIT_LIST_HEAD( &exited );
	INIT_LIST_HEAD( &wait );
	INIT_LIST_HEAD( &tasks );

	create_shell_process( "shell", (void *)ps, 0, 0, 0, 0 );

	register_interrupt_handler( 7, scheduler_tick );
	asm volatile(
	  "li $v0, 1000000\n\t"
	  "mtc0 $v0, $11\n\t"
	  "mtc0 $zero, $9" );

	//kernel_printf( "sched init complete\n" );
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
		//clear_tsk_need_resched( prev );

		//kernel_printf( "Check the nr %d\n", cfs_rq->nr_running );

		if ( unlikely( !cfs_rq->nr_running ) )
		{
			kernel_printf( "I am idle\n" );
			// if only one task idle
			break;
		}
		if ( cfs_rq->nr_running == 1 )
		{
			if ( countx++ == 0 )
			{
				//kernel_printf( "first here\n" );
				copy_context( &( cfs_rq->current_task->context ), pt_context );
			}

			break;
		}

		//kernel_printf( "kankan wo lai le ji ci\n" );

		//kernel_printf( "check the prev pid: %x\n", prev->pid );

		//kernel_printf( "check the prev pid: %d\n", prev->pid );
		put_prev_task_fair( cfs_rq, prev );
		next = pick_next_task( cfs_rq, prev );
		// kernel_printf( "prev: %x\n", prev->pid );
		// kernel_printf( "next: %x\n", next->pid );
		//kernel_printf( "check the sche pid: %d\n", next->pid );

		// if ( countx++ == 0 )
		// {
		// 	copy_context( &( next->context ), pt_context );
		// }
		//copy_context( &( cfs_rq->current_task->context ), pt_context );
		//kernel_printf( "check the next pid %x\n", next->pid );
		if ( likely( prev != next ) )
		{
			//kernel_printf( "we are not the same\n" );
			copy_context( pt_context, &( cfs_rq->current_task->context ) );

			cfs_rq->current_task = next;
			//cfs_rq->curr = &next->se;

			copy_context( &( cfs_rq->current_task->context ), pt_context );
			//context_switch!!
		}
		else
		{
			// if ( countx++ == 0 )
			// {
			// 	copy_context( &( cfs_rq->current_task->context ), pt_context );
			// 	//kernel_printf( "check the epc: %x\n", cfs_rq->current_task->context.epc );
			// }
			//kernel_printf( "choose the same\n" );
		}
		//kernel_printf( "Can be here\n" );
	} while ( need_resched() );
	// if ( countx++ == 1 )
	// {
	// 	//kernel_printf( "Kankan here\n" );
	// }
	//kernel_printf( "Cannot be here\n" );
	//enable_interrupts();
}

void scheduler_tick( unsigned int status, unsigned int cause, struct reg_context *pt_context )
{
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	struct task_struct *curr = cfs_rq->current_task;
	update_cfs_clock( cfs_rq );
	//time update

	task_tick_fair( cfs_rq, curr );

	// struct task_struct *prev, *next;
	// prev = cfs_rq->current_task;
	// put_prev_task_fair( cfs_rq, prev );
	// next = pick_next_task( cfs_rq, prev );

	// if ( countx++ == 0 )
	// {
	// 	copy_context( &( p->context ), pt_context );
	// }
	// kernel_printf( "kankan epc: %x", pt_context->epc );

	//task_tick_fair( cfs_rq, curr );

	scheduler( pt_context );
	//struct task_struct *prev, *next;
	//prev = cfs_rq->current_task;
	//put_prev_task_fair( cfs_rq, prev );
	//next = pick_next_task( cfs_rq, prev );
	// if ( countx++ == 0 )
	// {
	// 	copy_context( &( cfs_rq->current_task->context ), pt_context );
	// }

	asm volatile( "mtc0 $zero, $9\n\t" );
}

int task_fork( char *name, void ( *entry )( unsigned int argc, void *args ), unsigned int argc, void *args, int is_vm )
{
	union thread_union *new;
	//struct task_struct *p;
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	unsigned int init_gp;

	pid_t newpid = alloc_pidmap();

	if ( newpid == -1 )
	{
		free_pidmap( newpid );
		kernel_printf( "pid allocated failed\n" );
		return 0;
	}

	new = (union thread_union *)kmalloc( sizeof( union thread_union ) );

	//kernel_printf( "Down here1\n" );

	if ( new == 0 )
	{
		kernel_printf( "task_struct allocated failed\n" );
		return 0;
	}

	new->task.pid = newpid;
	new->task.ASID = newpid;
	new->task.parent = cfs_rq->current_task->pid;
	new->task.policy = SCHED_NORMAL;
	new->task.prioiry = NICE_TO_PRIO( 10 );
	new->task.THREAD_FLAG = 0;

	//kernel_memset( &( new->task.se ), 0, sizeof( struct sched_entity ) );

	sched_fork( &new->task );
	//p->se.exec_start = 0;
	//kernel_printf( "Down here2\n" );

	kernel_strcpy( new->task.name, name );

	kernel_memset( &( new->task.context ), 0, sizeof( struct reg_context ) );

	//kernel_printf( "Down here3\n" );

	new->task.context.epc = (unsigned int)entry;

	new->task.context.sp = (unsigned int)new + KERNEL_STACK_SIZE;

	asm volatile( "la %0, _gp\n\t"
				  : "=r"( init_gp ) );
	new->task.context.gp = init_gp;
	new->task.context.a0 = argc;
	new->task.context.a1 = (unsigned int)args;

	attach_pid( &new->task, newpid );

	// kernel_printf( "Down here4\n" );
	// kernel_printf( "check the epc in create: %x asd\n", new->task.context.epc );
	// kernel_printf( "check the execstart in create: %x asd\n", new->task.se.exec_start );

	add_task( &new->task );
	wake_up_new_task( &new->task );

	kernel_printf( "task: %x create\n", new->task.pid );
	return new->task.pid;
}

int test_vm( unsigned int argc, void *args )
{
	//kernel_printf( "I am here\n" );
	int countxx = 0;
	if ( kernel_strcmp( args, "loop" ) == 0 )
	{
		while ( 1 )
			;
	}
	for ( int i = 0; i < 1000000; i++ )
	{
		countxx++;
		if ( countxx % 100000 == 0 )
		{
			kernel_printf( "test_vm can do it\n" );
		}
	}
	do_exit();
}

int execk( unsigned int argc, void *args, int is_wait )
{
	pid_t newpid = task_fork( args, (void *)test_vm, argc, args, 0 );

	if ( is_wait )
	{
		waitpid( newpid );
	}
	return 1;
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

void do_exit()
{
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	struct task_struct *next;
	struct task_struct *current = cfs_rq->current_task;

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

	remove( current );
	add_exited( current );

	update_cfs_clock( cfs_rq );

	next = pick_next_task( cfs_rq, current );
	kernel_printf( "check the next pid in do_exit: %x\n", next->pid );

	if ( likely( current != next ) )
	{
		// copy_context( pt_context, &( cfs_rq->current_task->context ) );

		cfs_rq->current_task = next;
		cfs_rq->curr = &next->se;
		kernel_printf( "task exit\n" );
		switch_ex( &( cfs_rq->current_task->context ) );
	}

	kernel_printf( "Error: pc_exit\n" );
}

//1-success, 0-unsuccess
int pc_kill( pid_t pid )
{
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	if ( pid == 0 || pid == 1 || pid == cfs_rq->current_task->pid )
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
	remove( p );
	//add_exited( p );
	free_pidmap( pid );
	detach_pid( p );
	//my_cfs_rq.nr_running--;
	kfree( p );

	enable_interrupts();

	kernel_printf( "task: %x kill\n", pid );
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
	//add_wait( current );

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

void print_info()
{
	struct task_struct *next;
	struct list_head *pos;
	kernel_printf( "========CFS_RQ INFO=========\n" );
	kernel_printf( "Current task pid: %x\n", my_cfs_rq.current_task->pid );
	kernel_printf( "Current Systime time(10^-6): %x\n", my_cfs_rq.clock );
	kernel_printf( "Check the minumam vruntime: %x\n", my_cfs_rq.min_vruntime );
	kernel_printf( "Check the load weight in cfs_rq: %x\n", my_cfs_rq.load.weight );
	kernel_printf( "Check the number in rbtree: %d\n", my_cfs_rq.nr_running );
	kernel_printf( "========CFS_RQ INFO=========\n" );
	kernel_printf( "\n" );
	kernel_printf( "========TASK INFO=========\n" );
	list_for_each( pos, &tasks )
	{
		next = container_of( pos, struct task_struct, node );
		kernel_printf( "name:%s \t pid:%d \t  prio:%d \t\n", next->name, next->pid, next->prioiry );
	}
	kernel_printf( "========TASK INFO=========\n" );
	kernel_printf( "\n" );
	kernel_printf( "========SCHED INFO=========\n" );
	list_for_each( pos, &tasks )
	{
		next = container_of( pos, struct task_struct, node );
		kernel_printf( "vruntime:%x \t load weight:%x \t\n", next->se.vruntime, next->se.load.weight );
	}
	kernel_printf( "========SCHED INFO=========\n" );
}

static void set_priority( struct task_struct *p, long prioiry )
{
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	update_cfs_clock( cfs_rq );

	disable_interrupts();
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
	enable_interrupts();
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

void sys_prioiry_pid( int increment, int pid )
{
	long prioiry;
	//struct cfs_rq *cfs_rq = &my_cfs_rq;
	//struct task_struct *current = cfs_rq->current_task;
	struct task_struct *p = find_task_by_pid( pid );
	if ( p == NULL )
	{
		kernel_printf( "pid %d doesn't not exists\n", pid );
		return;
	}
	if ( increment < -40 )
		increment = -40;
	if ( increment > 40 )
		increment = 40;

	prioiry = p->prioiry + increment;
	if ( prioiry < 100 )
	{
		prioiry = 100;
	}
	if ( prioiry > 140 )
	{
		prioiry = 140;
	}

	set_priority( p, prioiry );
}