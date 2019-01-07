#include <kos/pc/sched.h>
#include <driver/vga.h>

#define WMULT_CONST ( ~0UL )
#define WMULT_SHIFT 32
#define LONG_MAX ( (long)( ~0UL >> 1 ) )
#define SRR( x, y ) ( ( ( x ) + ( 1UL << ( (y)-1 ) ) ) >> ( y ) )

unsigned int sysctl_sched_latency = 20000000ULL;
unsigned int sysctl_sched_min_granularity = 4000000ULL;
unsigned int sysctl_sched_wakeup_granularity = 10000000UL;
static unsigned int sched_nr_latency = 5;  //sysctl_sched_latency / sysctl_sched_min_granularity

static inline struct rb_node *first_fair( struct cfs_rq *cfs_rq )
{
	// if ( cfs_rq->rb_leftmost == NULL )
	// {
	// 	kernel_printf( "In first_fair\n" );
	// }
	return cfs_rq->rb_leftmost;
}

static struct sched_entity *__pick_next_entity( struct cfs_rq *cfs_rq )
{
	return rb_entry( first_fair( cfs_rq ), struct sched_entity, run_node );
}

static inline unsigned long max_vruntime( unsigned long max_vruntime, unsigned long vruntime )
{
	signed long delta = (signed long)( vruntime - max_vruntime );
	if ( delta > 0 )
		max_vruntime = vruntime;

	return max_vruntime;
}

static inline unsigned long min_vruntime( unsigned long min_vruntime, unsigned long vruntime )
{
	signed long delta = (signed long)( vruntime - min_vruntime );
	if ( delta < 0 )
		min_vruntime = vruntime;

	return min_vruntime;
}

static unsigned long calc_delta_mine( unsigned long delta_exec, struct load_weight *lw )
{
	unsigned long temp;
	if ( unlikely( !lw->inv_weight ) )
		lw->inv_weight = ( WMULT_CONST - lw->weight / 2 ) / lw->weight + 1;

	temp = (unsigned long)delta_exec * NICE_0_LOAD;

	if ( unlikely( temp > WMULT_CONST ) )
		temp = SRR( SRR( temp, WMULT_SHIFT / 2 ) * lw->inv_weight,
					WMULT_SHIFT / 2 );
	else
		temp = SRR( temp * lw->inv_weight, WMULT_SHIFT );

	kernel_printf( "check the temp: %x\n", temp );

	return (unsigned long)min( temp, (unsigned long)LONG_MAX );
}

static unsigned long sched_vslice_add( struct cfs_rq *cfs_rq, struct sched_entity *se )
{
	unsigned int nr_running = cfs_rq->nr_running + 1;
	unsigned long period = sysctl_sched_latency;  // 20ms
	unsigned long nr_latency = sched_nr_latency;  // 5

	if ( unlikely( nr_running > nr_latency ) )
	{
		period *= nr_running;
		do_div( period, nr_latency );
	}

	unsigned long vslice = period;
	vslice *= NICE_0_LOAD;
	do_div( vslice, cfs_rq->load.weight + se->load.weight );  //cfs_rq->load.weight don't know if needed!
	return vslice;
};

static void update_curr( struct cfs_rq *cfs_rq )
{
	struct sched_entity *curr = cfs_rq->curr;
	unsigned long now = cfs_rq->clock;  // ori from rq -> clock
	unsigned long delta_exec;
	//kernel_printf( "now in update_curr: %x\n", now );
	//kernel_printf( "check the curr execstart4: %d\n", my_cfs_rq.curr->exec_start );
	delta_exec = (unsigned long)( now - curr->exec_start );

	//kernel_printf( "1here ??\n" );

	unsigned long vruntime;

	curr->exec_max = max( (unsigned long)delta_exec, curr->exec_max );
	curr->sum_exec_runtime += delta_exec;

	//kernel_printf( "2here ??\n" );

	cfs_rq->exec_clock += delta_exec;
	if ( unlikely( curr->load.weight != NICE_0_LOAD ) )
		delta_exec = calc_delta_mine( delta_exec, &curr->load );
	curr->vruntime += delta_exec;

	//kernel_printf( "3here ??\n" );

	if ( first_fair( cfs_rq ) )
	{
		kernel_printf( "cannot be here\n" );
		vruntime = min_vruntime( curr->vruntime, __pick_next_entity( cfs_rq )->vruntime );
	}
	else
	{
		vruntime = curr->vruntime;
	}

	//kernel_printf( "cannot be here ??\n" );
	cfs_rq->min_vruntime = max_vruntime( cfs_rq->min_vruntime, vruntime );

	curr->exec_start = now;
	//kernel_printf( "Update_curr down\n" );
	//xgroup
}

static void place_entity( struct cfs_rq *cfs_rq, struct sched_entity *se, int initial )
{
	unsigned long vruntime;
	vruntime = cfs_rq->min_vruntime;
	if ( initial )
	{
		vruntime += sched_vslice_add( cfs_rq, se );
	}
	else
	{
		vruntime -= sysctl_sched_latency;
		vruntime = max_vruntime( se->vruntime, vruntime );
		// for waking up a task after sleep, we don't need to do this now.
	}
	se->vruntime = vruntime;
}

static inline unsigned long entity_key( struct cfs_rq *cfs_rq, struct sched_entity *se )
{
	return se->vruntime - cfs_rq->min_vruntime;
}

static void account_entity_enqueue( struct cfs_rq *cfs_rq, struct sched_entity *se )
{
	struct load_weight *lw = &cfs_rq->load;
	lw->weight += se->load.weight;
	cfs_rq->nr_running++;
	se->on_rq = 1;
}

static void account_entity_dequeue( struct cfs_rq *cfs_rq, struct sched_entity *se )
{
	struct load_weight *lw = &cfs_rq->load;
	lw->weight -= se->load.weight;
	cfs_rq->nr_running--;
	se->on_rq = 0;
}

static void __enqueue_entity( struct cfs_rq *cfs_rq, struct sched_entity *se )
{
	struct rb_node **link = &cfs_rq->tasks_timeline.rb_node;
	struct rb_node *parent = NULL;
	struct sched_entity *temp_entry;
	unsigned long key = se->vruntime - cfs_rq->min_vruntime;
	int leftmost = 1;

	while ( *link )
	{
		parent = *link;
		temp_entry = rb_entry( parent, struct sched_entity, run_node );
		if ( key < entity_key( cfs_rq, temp_entry ) )
		{
			link = &parent->rb_left;
		}
		else
		{
			link = &parent->rb_right;
			leftmost = 0;
		}

		if ( leftmost )
		{
			cfs_rq->rb_leftmost = &se->run_node;
		}
	}
	rb_link_node( &se->run_node, parent, link );
	rb_insert_color( &se->run_node, &cfs_rq->tasks_timeline );
}

static void enqueue_entity( struct cfs_rq *cfs_rq, struct sched_entity *se, int wakeup )
{
	update_curr( cfs_rq );
	if ( wakeup )
	{
		place_entity( cfs_rq, se, 0 );
	}

	if ( se != cfs_rq->curr )
	{
		se->wait_start = cfs_rq->clock;
	}

	if ( se != cfs_rq->curr )
	{
		//kernel_printf( "Into the _enqueue_entiyt\n" );
		__enqueue_entity( cfs_rq, se );
	}
	account_entity_enqueue( cfs_rq, se );
}

static void __dequeue_entity( struct cfs_rq *cfs_rq, struct sched_entity *se )
{
	if ( cfs_rq->rb_leftmost == &se->run_node )
		cfs_rq->rb_leftmost = rb_next( &se->run_node );

	rb_erase( &se->run_node, &cfs_rq->tasks_timeline );
}

static void dequeue_entity( struct cfs_rq *cfs_rq, struct sched_entity *se, int sleep )
{
	update_curr( cfs_rq );
	if ( se != cfs_rq->curr )
	{
		se->wait_max = max( se->wait_max, cfs_rq->clock - se->wait_start );
		se->wait_start = 0;
		__dequeue_entity( cfs_rq, se );
	}

	if ( sleep )
	{
		//
	}
	account_entity_dequeue( cfs_rq, se );
}

void enqueue_task_fair( struct cfs_rq *cfs_rq, struct task_struct *p, int wakeup )
{
	struct sched_entity *se = &p->se;

	for_each_sched_entity( se )
	{
		if ( se->on_rq )
			break;
		enqueue_entity( cfs_rq, se, wakeup );
		wakeup = 1;
	}
}

static inline void resched_task( struct task_struct *p )
{
	p->THREAD_FLAG = TIF_NEED_RESCHED;
	//task_thread_info( p )->flag = TIF_NEED_RESCHED;
}

static unsigned long sched_slice( struct cfs_rq *cfs_rq, struct sched_entity *se )
{
	unsigned long slice = sysctl_sched_latency;
	unsigned long nr_latency = sched_nr_latency;
	if ( unlikely( cfs_rq->nr_running > nr_latency ) )
	{
		slice *= cfs_rq->nr_running;
		do_div( slice, nr_latency );
	}

	slice *= se->load.weight;
	do_div( slice, cfs_rq->load.weight );

	return slice;
}

static void check_preempt_tick( struct cfs_rq *cfs_rq, struct sched_entity *curr )
{
	unsigned long ideal_runtime, delta_exec;

	ideal_runtime = sched_slice( cfs_rq, curr );
	delta_exec = curr->sum_exec_runtime - curr->prev_sum_exec_runtime;
	if ( delta_exec > ideal_runtime )
		resched_task( cfs_rq->current_task );
}

static void entity_tick( struct cfs_rq *cfs_rq, struct sched_entity *curr )
{
	update_curr( cfs_rq );

	if ( cfs_rq->nr_running > 1 )
		check_preempt_tick( cfs_rq, curr );
}

static void put_prev_entity( struct cfs_rq *cfs_rq, struct sched_entity *prev )
{
	if ( prev->on_rq )
	{
		update_curr( cfs_rq );
		prev->wait_start = cfs_rq->clock;
		__enqueue_entity( cfs_rq, prev );
	}
	cfs_rq->current_task = NULL;
}

static void set_next_entity( struct cfs_rq *cfs_rq, struct sched_entity *se )
{
	if ( se->on_rq )
	{
		se->wait_max = max( se->wait_max, cfs_rq->clock - se->wait_start );
		se->wait_start = 0;
		__dequeue_entity( cfs_rq, se );
	}

	//kernel_printf( "I am hehre !QSDAsd\n" );
	se->exec_start = cfs_rq->clock;
	cfs_rq->curr = se;

	se->prev_sum_exec_runtime = se->sum_exec_runtime;
}

static struct sched_entity *pick_next_entity( struct cfs_rq *cfs_rq )
{
	struct sched_entity *se = NULL;
	//kernel_printf( "the name: %s", cfs_rq->current_task->name );
	if ( first_fair( cfs_rq ) )
	{
		se = __pick_next_entity( cfs_rq );
		set_next_entity( cfs_rq, se );
	}
	else
	{
		kernel_printf( "Here returns a null in pick_next_entity\n" );
		while ( 1 )
			;
	}

	return se;
}

static inline struct task_struct *task_of( struct sched_entity *se )
{
	return container_of( se, struct task_struct, se );
}

void check_preempt_wakeup( struct cfs_rq *cfs_rq, struct task_struct *p )
{
	struct task_struct *curr = cfs_rq->current_task;
	struct sched_entity *se = &curr->se, *pse = &p->se;

	unsigned long gran = sysctl_sched_wakeup_granularity;
	if ( unlikely( se->load.weight != NICE_0_LOAD ) )
		gran = calc_delta_mine( gran, &se->load );

	if ( pse->vruntime + gran < se->vruntime )
	{
		resched_task( curr );
	}
}

void task_new_fair( struct cfs_rq *cfs_rq, struct task_struct *p )
{
	struct sched_entity *se = &p->se, *curr = cfs_rq->curr;
	//kernel_printf( "check the curr execstart3: %d\n", my_cfs_rq.curr->exec_start );
	//kernel_printf( "check the curr execstart3: %d\n", cfs_rq->curr->exec_start );
	update_curr( cfs_rq );
	place_entity( cfs_rq, se, 1 );

	if ( curr && curr->vruntime < se->vruntime )
	{
		swap( curr->vruntime, se->vruntime );
	}
	enqueue_task_fair( cfs_rq, p, 0 );
	resched_task( cfs_rq->current_task );

	//kernel_printf( "task_new_fair down\n" );
}

void task_tick_fair( struct cfs_rq *cfs_rq, struct task_struct *curr )
{
	struct sched_entity *se = &curr->se;

	for_each_sched_entity( se )
	{
		entity_tick( cfs_rq, se );
	}
}

void put_prev_task_fair( struct cfs_rq *cfs_rq, struct task_struct *prev )
{
	struct sched_entity *se = &prev->se;
	for_each_sched_entity( se )
	{
		put_prev_entity( cfs_rq, se );
	}
}

struct task_struct *pick_next_task_fair( struct cfs_rq *cfs_rq )
{
	struct sched_entity *se;
	if ( unlikely( !cfs_rq->nr_running ) )
	{
		return NULL;
	}
	se = pick_next_entity( cfs_rq );
	return task_of( se );
}

void dequeu_task_fair( struct cfs_rq *cfs_rq, struct task_struct *p, int sleep )
{
	struct sched_entity *se = &p->se;
	for_each_sched_entity( se )
	{
		dequeue_entity( cfs_rq, se, sleep );
		/* Don't dequeue parent if it has other entities besides us */
		// if ( cfs_rq->load.weight )
		// 	break;
		sleep = 1;
	}
}
//task_tick_fair会被时钟中断触发，每次出发时会检查是否当前运行的进程需要调度，若需要则会置一个flag，我希望实现在中断返回时调用schedule，执行进程的切换。
// 有三个时间变量还未定义