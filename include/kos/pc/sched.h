#ifndef _SCHED_H
#define _SCHED_H

#include <kos/pc/rbtree.h>
#include <kos/kernel.h>
#include <kos/pc/pid.h>
#include <kos/vm/vmm.h>

#define TASK_RUNNING 0  //进程要么正在执行，要么准备执行
#define TASK_READY 1
#define TASK_DEAD 2		//死亡
#define TASK_WAITING 3  //唤醒进程
#define TASK_RUNNING_IDLE 4

#define TASK_NAME_LEN 32

// // register struct thread_info *__current_thread_info __asm__( "$28" );  //这个感觉有问题，不能定义在这里
// #define current_thread_info() __current_thread_info
// #define get_current() ( current_thread_info()->task )
// #define current get_current()

// #define get_cfs() &( rq.cfs )
extern struct cfs_rq my_cfs_rq;
extern struct task_struct idle_task;
extern int xxxxxxxxxxxxxxx;

//#define current() ( my_cfs_rq.current_task )

#define NICE_0_LOAD 1024
#define MAX_RT_PRIO 100
#define NICE_TO_PRIO( nice ) ( MAX_RT_PRIO + ( nice ) + 20 )
#define PRIO_TO_NICE( prio ) ( (prio)-MAX_RT_PRIO - 20 )

#define TIF_NEED_RESCHED 3
#define SCHED_IDLE 5
#define SCHED_NORMAL 8
#define WEIGHT_IDLEPRIO 2
#define WMULT_IDLEPRIO ( 1 << 31 )

#define KERNEL_STACK_SIZE 4096

#define CLOCK_INTERRUPTER_TICK 10  //msec

static const int prio_to_weight[ 40 ] = {
	/* -20 */ 88761,
	71755,
	56483,
	46273,
	36291,
	/* -15 */ 29154,
	23254,
	18705,
	14949,
	11916,
	/* -10 */ 9548,
	7620,
	6100,
	4904,
	3906,
	/*  -5 */ 3121,
	2501,
	1991,
	1586,
	1277,
	/*   0 */ 1024,
	820,
	655,
	526,
	423,
	/*   5 */ 335,
	272,
	215,
	172,
	137,
	/*  10 */ 110,
	87,
	70,
	56,
	45,
	/*  15 */ 36,
	29,
	23,
	18,
	15,
};

static const unsigned int prio_to_wmult[ 40 ] = {
	/* -20 */ 48388,
	59856,
	76040,
	92818,
	118348,
	/* -15 */ 147320,
	184698,
	229616,
	287308,
	360437,
	/* -10 */ 449829,
	563644,
	704093,
	875809,
	1099582,
	/*  -5 */ 1376151,
	1717300,
	2157191,
	2708050,
	3363326,
	/*   0 */ 4194304,
	5237765,
	6557202,
	8165337,
	10153587,
	/*   5 */ 12820798,
	15790321,
	19976592,
	24970740,
	31350126,
	/*  10 */ 39045157,
	49367440,
	61356676,
	76695844,
	95443717,
	/*  15 */ 119304647,
	148102320,
	186737708,
	238609294,
	286331153,
};

struct reg_context
{
	unsigned int epc;
	unsigned int at;
	unsigned int v0, v1;
	unsigned int a0, a1, a2, a3;
	unsigned int t0, t1, t2, t3, t4, t5, t6, t7;
	unsigned int s0, s1, s2, s3, s4, s5, s6, s7;
	unsigned int t8, t9;
	unsigned int hi, lo;
	unsigned int gp;
	unsigned int sp;
	unsigned int fp;
	unsigned int ra;
};

struct load_weight
{
	unsigned long weight, inv_weight;
};

struct cfs_rq
{
	struct load_weight load;
	unsigned long nr_running;
	unsigned long exec_clock;
	unsigned long min_vruntime;

	struct rb_root tasks_timeline;
	struct rb_node *rb_leftmost;
	struct rb_node *rb_load_balance_curr;

	struct sched_entity *curr;
	struct task_struct *current_task;
	struct task_struct *idle;

	unsigned long clock;
	unsigned long prev_clock_raw;
	unsigned long clock_max_delta;
};

// struct rq
// {
// 	unsigned long clock;
// 	struct cfs_rq cfs;
// };

struct sched_entity
{
	struct load_weight load; /* for load-balancing */
	struct rb_node run_node;
	unsigned int on_rq;

	unsigned long exec_start;
	unsigned long sum_exec_runtime;
	unsigned long vruntime;
	unsigned long prev_sum_exec_runtime;
	unsigned long exec_max;
	unsigned long wait_start;
	unsigned long wait_max;

	struct cfs_rq *cfs_rq;
};

struct task_struct
{
	pid_t pid;
	pid_t parent;
	struct pid pids;
	volatile long state;
	unsigned char name[ TASK_NAME_LEN ];

	// struct list_head tasks;
	struct mm_struct *mm;
	unsigned int ASID;

	// struct task_struct *parent;
	// struct list_head children; /* list of my children */
	// struct list_head sibling;  /* linkage in my parent's children list */
	int prioiry;
	unsigned int policy;

	unsigned int THREAD_FLAG;

	struct sched_entity se;
	struct reg_context context;

	struct list_head node;
};

struct thread_info
{
	struct task_struct *task;  // may be wrong, because I am not sure if this thread_info should exits
};

union thread_union
{
	struct task_struct *task;
	//unsigned long stack[ PAGE_SIZE / sizeof( long ) ];
};

// #define INIT_TASK( tsk )            \
// 	{                               \
// 		.name = "idle",             \
// 		.state = TASK_RUNNING_IDLE, \
// 		.THREAD_FLAG = 0,           \
// 		.pid = 0,                   \
// 		.parent = 0,                \
// 		.policy = SCHED_IDLE,       \
// 		.prioiry = 120,             \
// 	}

// #define INIT_CFS_RQ( cfs_rq )                                   \
// 	{                                                           \
// 		.tasks_timeline = RB_ROOT,                              \
// 		.min_vruntime = (unsigned long long)( -( 1LL << 20 ) ), \
// 		.exec_clock = 0,                                        \
// 		.nr_running = 0,                                        \
// 		.clock = 1,                                             \
// 		.prev_clock_raw = 0,                                    \
// 		.clock_max_delta = 0,                                   \
// 		.load.weight = 0,                                       \
// 	}

void scheduler_tick( unsigned int status, unsigned int cause, struct reg_context *pt_context );
void task_tick_fair( struct cfs_rq *cfs_rq, struct task_struct *curr );
void task_new_fair( struct cfs_rq *cfs_rq, struct task_struct *p );
void check_preempt_wakeup( struct cfs_rq *cfs_rq, struct task_struct *p );
void enqueue_task_fair( struct cfs_rq *cfs_rq, struct task_struct *p, int wakeup );
void dequeu_task_fair( struct cfs_rq *cfs_rq, struct task_struct *p, int sleep );
void put_prev_task_fair( struct cfs_rq *cfs_rq, struct task_struct *prev );
struct task_struct *pick_next_task_fair( struct cfs_rq *cfs_rq );
void sys_prioiry( int increment );
extern void switch_ex( struct reg_context *regs );
extern void switch_wa( struct reg_context *des, struct reg_context *src );
int task_fork( char *name, void ( *entry )( unsigned int argc, void *args ), unsigned int argc, void *args, pid_t *retpid, int is_vm );
int execk( unsigned int argc, void *args, int is_wait );
void do_exit();
int pc_kill( pid_t pid );
void waitpid( pid_t pid );
void sched_init();

int xxx;
#endif