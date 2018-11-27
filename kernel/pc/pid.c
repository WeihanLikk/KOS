#include <kos/sched.h>

#define pid_hashfn(x) hash_long((unsigned long) x, pidhash_shift)
static struct hlist_head *pid_hash;
static int pidhash_shift = 11;

int pid_max = PID_MAX_DEFAULT;

void pidhash_initial()
{
    int pidhash_size = 1 << pidhash_shift; //2048
    pid_hash = alloc_bootmem(pidhash_size * sizeof(*(pid_hash)));
    if(!pid_hash)
        kernel_printf("Could not alloc pidhash!\n");
    for(int i = 0; i < pidhash_size; i++)
        INIT_HLIST_HEAD(&pid_hash[i]);
}

struct pid* find_pid(int nr)
{
    struct hlist_node *elem;
	struct pid *pid;

    hlist_for_each_entry(pid, elem, &pid_hash[pid_hashfn(nr)], pid_chain)
    {
        if(pid -> nr == nr)
            return pid;
    }
    return NULL;
}

tast_t *find_task_by_pid(int nr)
{
    struct pid* pid;
    pid = find_pid(nr);
    if(pid == NULL)
    {
        return NULL;
    }
    return pid_task(&pid->pid_list, type);
    
}