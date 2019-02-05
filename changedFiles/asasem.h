/*
 * Copyright (c) 2014 ASASoft
 */

#ifndef __LINUX_ASASEM_H
#define __LINUX_ASASEM_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ftrace.h>

/* Please don't access any members of this structure directly */
struct asasem {
	spinlock_t		lock;
	unsigned int		count;
	unsigned int		capacity;
	struct list_head	wait_list;
	struct asasem_active	*active_arr;
};

struct asasem_waiter {
	struct list_head list;
	struct task_struct *task;
	int up;
};

struct asasem_active {
	struct task_struct *task;
	int original_priority;
};

#define __ASASEM_INITIALIZER(name, n)					\
{									\
	.lock		= __SPIN_LOCK_UNLOCKED((name).lock),		\
	.count		= n,						\
	.capacity	= n,						\
	.wait_list	= LIST_HEAD_INIT((name).wait_list),		\
	.active_arr	=(struct asasem_active *)((void*)(sem)+sizeof(struct asasem))\
}

#define __ASASEM_ACTIVE_INITIALIZER(task_ptr, prio)			\
{									\
	.task			=	(task_ptr),			\
	.original_priority	=	(prio)				\
}

#define PriorityOfTaskPtr(p)	(p)->static_prio

asmlinkage long sys_asasem_init(void *asasem_voidptr, int n) ;
asmlinkage long sys_asasem_wait(void *asasem_voidptr) ;
asmlinkage long sys_asasem_signal(void *asasem_voidptr) ;

#endif /* __LINUX_SEMAPHORE_H */
