#include <kos/sched.h>
#include <kos/kernel.h>

#define WMULT_CONST ( ~0UL )
#define WMULT_SHIFT 32
#define LONG_MAX ( (long)( ~0UL >> 1 ) )
#define SRR( x, y ) ( ( ( x ) + ( 1UL << ( (y)-1 ) ) ) >> ( y ) )

unsigned int sysctl_sched_latency = 20000000ULL;
unsigned int sysctl_sched_min_granularity = 4000000ULL;
static unsigned int sched_nr_latency = 5;  //sysctl_sched_latency / sysctl_sched_min_granularity

static inline struct rb_node *first_fair( struct cfs_rq *cfs_rq )
{
	return cfs_rq->rb_leftmost;
}

static struct sched_entity *__pick_next_entity( struct cfs_rq *cfs_rq )
{
	return rb_entry( first_fair( cfs_rq ), struct sched_entity, run_node );
}

static inline unsigned long long max_vruntime( unsigned long long max_vruntime, unsigned long long vruntime )
{
	signed long long delta = (signed long long)( vruntime - max_vruntime );
	if ( delta > 0 )
		max_vruntime = vruntime;

	return max_vruntime;
}

static inline unsigned long long min_vruntime( unsigned long long min_vruntime, unsigned long long vruntime )
{
	signed long long delta = (signed long long)( vruntime - min_vruntime );
	if ( delta < 0 )
		min_vruntime = vruntime;

	return min_vruntime;
}

static unsigned long calc_delta_mine( unsigned long delta_exec, struct load_weight *lw )
{
	unsigned long long temp;
	if ( unlikely( !lw->inv_weight ) )
		lw->inv_weight = ( WMULT_CONST - lw->weight / 2 ) / lw->weight + 1;

	temp = (unsigned long long)delta_exec * 1024;

	if ( unlikely( temp > WMULT_CONST ) )
		temp = SRR( SRR( temp, WMULT_SHIFT / 2 ) * lw->inv_weight,
					WMULT_SHIFT / 2 );
	else
		temp = SRR( temp * lw->inv_weight, WMULT_SHIFT );

	return (unsigned long)min( temp, (unsigned long long)(unsigned long)LONG_MAX );
}

static unsigned long long sched_vslice_add( struct cfs_rq *cfs_rq, struct sched_entity *se )
{
	unsigned long nr_running = cfs_rq->nr_running + 1;
	unsigned long long period = sysctl_sched_latency;  // 20ms
	unsigned long nr_latency = sched_nr_latency;	   // 5

	if ( unlikely( nr_running > nr_latency ) )
	{
		period *= nr_running;
		do_div( period, nr_latency );
	}

	unsigned long long vslice = period;
	vslice *= NICE_0_LOAD;
	do_div( vslice, cfs_rq->load.weight + se->load.weight );  //cfs_rq->load.weight don't know if needed!
	return vslice;
};

static void update_curr( struct cfs_rq *cfs_rq )
{
	struct sched_entity *curr = cfs_rq->curr;
	unsigned long long now = cfs_rq->clock;  // ori from rq -> clock
	unsigned long delta_exec;
	delta_exec = (unsigned long)( now - curr->exec_start );

	unsigned long long vruntime;

	curr->sum_exec_runtime += delta_exec;
	cfs_rq->exec_clock += delta_exec;
	if ( unlikely( curr->load.weight != NICE_0_LOAD ) )
		delta_exec = calc_delta_mine( delta_exec, &curr->load );
	curr->vruntime += delta_exec;

	if ( first_fair( cfs_rq ) )
	{
		vruntime = min_vruntime( curr->vruntime, __pick_next_entity( cfs_rq )->vruntime );
	}
	else
	{
		vruntime = curr->vruntime;
	}

	cfs_rq->min_vruntime = max_vruntime( cfs_rq->min_vruntime, vruntime );

	curr->exec_start = now;
	//xgroup
}

static void place_entity( struct cfs_rq *cfs_rq, struct sched_entity *se, int initial )
{
	unsigned long long vruntime;
	vruntime = cfs_rq->min_vruntime;
	if ( initial )
	{
		vruntime += sched_vslice_add( cfs_rq, se );
	}
	else
	{
		// for waking up a task after sleep, we don't need to do this now.
	}
	se->vruntime = vruntime;
}

static void task_new_fair( struct task_struct *p )
{
	struct cfs_rq *cfs_rq = get_cfs();
	struct sched_entity *se = &p->se, *curr = cfs_rq->curr;

	update_curr( cfs_rq );
	place_entity( cfs_rq, se, 1 );

	if ( curr && curr->vruntime < se->vruntime )
	{
		swap( curr->vruntime, se->vruntime );
	}

	// hai mei wan 1086
	// enqueue_task_fair(rq, p, 0);
	// resched_task(rq->curr);
}