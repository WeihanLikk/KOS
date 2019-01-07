#include <kos/pc/sched.h>
//#include <kos/vm/vmm.h>
#include <driver/vga.h>
#include <intr.h>
#include <kos/log.h>
#include <kos/slab.h>
#include <kos/ps.h>

struct list_head wait;
struct list_head exited;

struct cfs_rq my_cfs_rq;
struct task_struct idle_task;
struct task_struct *current_task = 0;
int countx = 0;
unsigned long total = 0;

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
	if ( p->THREAD_FLAG == (unsigned int)TIF_NEED_RESCHED )
	{
		//kernel_printf( "???? be here111\n" );
		return 1;
	}
	else
	{
		//kernel_printf( "Must be here222\n" );
		return 0;
	}
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

	p->prioiry = NICE_TO_PRIO( 0 );
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
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	update_cfs_clock( cfs_rq );

	task_new_fair( cfs_rq, p );

	//cfs_rq->nr_running++;
	cfs_rq->load.weight += p->se.load.weight;

	check_preempt_wakeup( cfs_rq, p );
}

// void init_idle( struct task_struct *idle )
// {
// 	//
// 	struct cfs_rq *cfs_rq = &my_cfs_rq;
// 	cfs_rq->idle = idle;
// 	cfs_rq->idle->pid = alloc_pidmap();
// 	if ( cfs_rq->idle->pid != 0 )
// 	{
// 		//log( LOG_OK, "Here ka zhu le." );
// 		kernel_printf( "idle pid error\n" );
// 		kernel_printf( "The idle pid: %d\n", cfs_rq->idle->pid );
// 	}
// }

int testtest()
{
	//int countxx = 0;
	kernel_printf( "HERe !!!!!!!!!\n" );
	// for ( int i = 0; i < 10000000; i++ )
	// {
	// 	countxx++;
	// 	if ( countxx == 10000 )
	// 	{
	// 		kernel_printf( "I can do it\n" );
	// 	}
	// }
}

int asdasd()
{
	kernel_printf( "asdasdasd !!!!!!!!!\n" );
	int countxx = 0;
	for ( int i = 0; i < 10000000; i++ )
	{
		countxx++;
		if ( countxx % 1000 == 0 )
		{
			kernel_printf( "I can do it\n" );
		}
	}
}

static void create_shell_process( char *name, void ( *entry )( unsigned int argc, void *args ), unsigned int argc, void *args, pid_t *returnpid, int is_vm )
{
	struct task_struct *p;
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	unsigned int init_gp;

	pid_t newpid = alloc_pidmap();

	if ( newpid == -1 )
	{
		free_pidmap( newpid );
		kernel_printf( "pid allocated failed\n" );
		return;
	}
	// union thread_union *new;
	// new = (union thread_union *)kmalloc( sizeof( union thread_union ) );
	// if ( new == 0 )
	// {
	// 	kernel_printf( "new allocated failed\n" );
	// 	while ( 1 )
	// 		;
	// }
	// new->task.pid = 1;
	// new->task.parent = 0;
	// new->task.policy = SCHED_NORMAL;
	// new->task.prioiry = cfs_rq->idle->prioiry;
	// new->task.THREAD_FLAG = 0;

	// kernel_memset( &( new->task.se ), 0, sizeof( struct sched_entity ) );
	// sched_fork( &new->task );
	// //p->se.exec_start = 0;
	// kernel_printf( "Down here2\n" );

	// kernel_strcpy( new->task.name, "shell" );

	// kernel_memset( &( new->task.context ), 0, sizeof( struct reg_context ) );

	// kernel_printf( "Down here3\n" );

	// //void( *entry ) = (void *)ps;
	// //void( *entry ) = (void *)testtest;
	// new->task.context.epc = (unsigned int)entry;
	// new->task.context.gp = (unsigned int)p;
	// asm volatile( "la %0, _gp\n\t"
	// 			  : "=r"( init_gp ) );
	// new->task.context.gp = init_gp;
	// new->task.context.a0 = argc;
	// new->task.context.a1 = (unsigned int)args;
	// //attach_pid( p, newpid );

	// kernel_printf( "Down here4\n" );
	// // kernel_printf( "check the epc in create: %x asd\n", p->context.epc );
	// // kernel_printf( "check the execstart in create: %x asd\n", p->se.exec_start );

	// my_cfs_rq.current_task = &new->task;
	// my_cfs_rq.curr = &new->task.se;
	// current_task = my_cfs_rq.current_task;
	// kernel_printf( "kernel shell created\n" );

	p = (struct task_struct *)kmalloc( sizeof( struct task_struct ) );
	//p = (struct task_struct *)( (unsigned int)a | 0x80000000 );
	//x = (int *)( (unsigned int)y | 0x80000000 );

	kernel_printf( "Down here1\n" );

	if ( p == 0 )
	{
		kernel_printf( "task_struct allocated failed\n" );
		return;
	}

	p->pid = newpid;
	p->parent = cfs_rq->idle->pid;
	p->policy = SCHED_NORMAL;
	p->prioiry = cfs_rq->idle->prioiry;
	p->THREAD_FLAG = 0;

	kernel_memset( &( p->se ), 0, sizeof( struct sched_entity ) );
	sched_fork( p );
	//p->se.exec_start = 0;
	kernel_printf( "Down here2\n" );

	kernel_strcpy( p->name, "shell" );

	kernel_memset( &( p->context ), 0, sizeof( struct reg_context ) );

	kernel_printf( "Down here3\n" );

	//void( *entry ) = (void *)ps;
	//void( *entry ) = (void *)testtest;
	p->context.epc = (unsigned int)entry;
	p->context.gp = (unsigned int)p;
	asm volatile( "la %0, _gp\n\t"
				  : "=r"( init_gp ) );
	p->context.gp = init_gp;
	p->context.a0 = argc;
	p->context.a1 = (unsigned int)args;
	//attach_pid( p, newpid );

	kernel_printf( "Down here4\n" );
	kernel_printf( "check the epc in create: %x asd\n", p->context.epc );
	kernel_printf( "check the execstart in create: %x asd\n", p->se.exec_start );

	// my_cfs_rq.current_task = p;
	// my_cfs_rq.curr = &p->se;
	// current_task = my_cfs_rq.current_task;
	//my_cfs_rq.tasks_timeline.rb_node = &my_cfs_rq.curr->run_node;
	// kernel_printf( "check the context %d\n", p->context.a1 );
	// kernel_printf( "check the pid: %d\n", p->pid );
	// //kernel_printf( "check the curr execstart0: %d\n", p->se.exec_start );
	// kernel_printf( "check the curr execstart0.5: %d\n", my_cfs_rq.current_task->se.exec_start );
	// kernel_printf( "check the curr execstart1: %d\n", my_cfs_rq.curr->exec_start );
	wake_up_new_task( p );
	kernel_printf( "kernel shell created\n" );
}

static void init_task( struct task_struct *p )
{
	kernel_strcpy( p->name, "idle" );
	p->state = TASK_RUNNING_IDLE;
	p->THREAD_FLAG = 0;
	p->pid = alloc_pidmap();
	p->parent = 0;
	p->policy = SCHED_NORMAL;
	p->prioiry = 120;
	kernel_memset( &( p->se ), 0, sizeof( struct sched_entity ) );
	set_load_weight( p );
	kernel_memset( &( p->context ), 0, sizeof( struct reg_context ) );

	void( *entry ) = (void *)asdasd;
	unsigned int init_gp;
	p->context.epc = (unsigned int)entry;
	p->context.gp = (unsigned int)p;
	asm volatile( "la %0, _gp\n\t"
				  : "=r"( init_gp ) );
	p->context.gp = init_gp;
	p->context.a0 = 0;
	p->context.a1 = (unsigned int)0;
}

static void init_cfs_rq( struct cfs_rq *rq )
{
	struct rb_root root;
	root.rb_node = NULL;
	rq->tasks_timeline = root;
	rq->min_vruntime = (unsigned long)( -( 1LL << 20 ) );
	rq->exec_clock = 0;
	rq->nr_running = 0;
	rq->clock = 1;
	rq->prev_clock_raw = 0;
	rq->clock_max_delta = 0;
	//rq->load.weight = 0;
	rq->rb_leftmost = NULL;
	rq->rb_load_balance_curr = NULL;
	//kernel_printf( "check rq: %x\n", rq->clock );
	kernel_memset( &( rq->load ), 0, sizeof( struct load_weight ) );
	rq->idle = &idle_task;
	rq->current_task = current_task = &idle_task;
	rq->curr = &idle_task.se;
}

void sched_init()
{
	pidhash_initial();
	pidmap_init();

	init_task( &idle_task );
	init_cfs_rq( &my_cfs_rq );

	//idle_task = init_task();
	//my_cfs_rq = init_cfs_rq();
	// init_task = INIT_TASK( init_task );
	// my_cfs_rq = INIT_CFS_RQ( my_cfs_rq );

	//set_load_weight( &init_task );
	//init_idle( &init_task );
	//my_cfs_rq.current_task = &init_task;

	INIT_LIST_HEAD( &exited );
	INIT_LIST_HEAD( &wait );

	wake_up_new_task( &idle_task );
	kernel_printf( "idle wake up\n" );

	create_shell_process( "shell", (void *)testtest, 0, 0, 0, 0 );

	register_interrupt_handler( 7, scheduler_tick );
	asm volatile(
	  "li $v0, 1000000\n\t"
	  "mtc0 $v0, $11\n\t"
	  "mtc0 $zero, $9" );

	kernel_printf( "sched init down\n" );
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

		//kernel_printf( "Check the nr %d\n", cfs_rq->nr_running );

		if ( unlikely( !cfs_rq->nr_running ) )
		{
			kernel_printf( "I am idle\n" );
			// if only one task idle
			break;
		}

		//kernel_printf( "kankan wo lai le ji ci\n" );

		//kernel_printf( "check the prev pid: %x\n", prev->pid );

		put_prev_task_fair( cfs_rq, prev );
		next = pick_next_task( cfs_rq, prev );

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
		//kernel_printf( "Can be here\n" );
	} while ( need_resched() );
	//kernel_printf( "Kankan here\n" );
	//kernel_printf( "Cannot be here\n" );
	//enable_interrupts();
}

void scheduler_tick( unsigned int status, unsigned int cause, struct reg_context *pt_context )
{
	struct cfs_rq *cfs_rq = &my_cfs_rq;
	struct task_struct *curr = cfs_rq->current_task;
	//update_cfs_clock( cfs_rq );
	//time update

	if ( countx++ == 0 )
	{
		copy_context( &( cfs_rq->current_task->context ), pt_context );
	}
	// kernel_printf( "kankan epc: %x", pt_context->epc );

	//task_tick_fair( cfs_rq, curr );

	//scheduler( pt_context );
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
		//p->mm = mm_create();
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