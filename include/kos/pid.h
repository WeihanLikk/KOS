#ifndef _PID_H
#define _PID_H

#include <kos/hlist.h>
#include <kos/list.h>

#define PID_MAX_DEFAULT 0x8000
#define pid_task( elem ) \
	list_entry( elem, struct task_struct, pids.pid_list )

struct pidmap
{
	int nr_free;
	void *page;
};

struct pid
{
	struct list_head pid_list;	//指回 pid_link 的 node
	int nr;						  //PID
	struct hlist_node pid_chain;  //pid hash 散列表结点
};

#endif