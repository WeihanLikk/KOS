#ifndef _SCHED_H
#define _SCHED_H

#include <kos/pid.h>

#define TASK_RUNNING        0//进程要么正在执行，要么准备执行
#define TASK_INTERRUPTIBLE  1 //可中断的睡眠，可以通过一个信号唤醒
#define TASK_UNINTERRUPTIBLE    2 //不可中断睡眠，不可以通过信号进行唤醒
#define __TASK_STOPPED      4 //进程停止执行
#define __TASK_TRACED       8 //进程被追踪
/* in tsk->exit_state */ 
#define EXIT_ZOMBIE     16 //僵尸状态的进程，表示进程被终止，但是父进程还没       有获取它的终止信息，比如进程有没有执行完等信息。                     
#define EXIT_DEAD       32 //进程的最终状态，进程死亡。
/* in tsk->state again */ 
#define TASK_DEAD       64 //死亡
#define TASK_WAKEKILL       128 //唤醒并杀死的进程
#define TASK_WAKING     256 //唤醒进程 

register struct thread_info *__current_thread_info __asm__("$28");
#define current_thread_info()  __current_thread_info
#define get_current()	(current_thread_info()->task)
#define current		get_current()

typedef struct task_struct task_t;
typedef int pid_t;

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

struct task_struct
{
    volatile long state;
    void *stack;

    struct list_head tasks;
    struct mm_struct *mm;

    struct task_struct *parent;
    struct list_head children;	/* list of my children */
	struct list_head sibling;	/* linkage in my parent's children list */

    struct reg_context context;

    pid_t _pid;
    struct pid pids;
  
};

struct thread_info
{
    struct task_struct *task;
};

union thread_union {
	struct thread_info thread_info;
	unsigned long stack[PAGE_SIZE/sizeof(long)];
};

#endif