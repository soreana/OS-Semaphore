/*
 * Copyright (c) 2014 ASASoft
 */

#include <linux/asasem.h>

static int __sched sleepInQueue(struct asasem *sem, long state, long timeout);
static int __sched wakeUpNext(struct asasem *sem);
static inline struct asasem_waiter* mostPriorWaiterInWaitList(struct list_head *lh);
static inline int addInActiveQueue(struct task_struct *p, struct asasem *sem);
static inline int removeFromActiveQueue(struct task_struct *p, struct asasem *sem);
static inline int active_IdxByTaskAdr(struct task_struct *p, struct asasem *sem);
static inline int updatePrioOfActives(struct asasem *sem);
static inline int printTaskInfo(struct task_struct *p);
static inline int printActiveList(struct asasem *sem, int size);
static inline int numOfActiveProcs(struct asasem *sem);		//CAUTION: result not valid in some cases: see the definition
static inline int higherPrio(struct task_struct *p1, struct task_struct *p2);

asmlinkage long sys_asasem_init(void *asasem_voidptr, int n)
{
	struct asasem* sem=(struct asasem *)asasem_voidptr;
	static struct lock_class_key __key;
	if (n<=0)
		return -1 ;

	*sem = (struct asasem) __ASASEM_INITIALIZER(*sem, n);
	lockdep_init_map(&sem->lock.dep_map, "semaphore->lock", &__key, 0);

	return 0 ;
}

asmlinkage long sys_asasem_wait(void *asasem_voidptr) //KILLABLE WAIT
{
	unsigned long flags;
	int result = 0;
	struct asasem* sem=(struct asasem *)asasem_voidptr;

	spin_lock_irqsave(&sem->lock, flags);
	if (likely(sem->count > 0))
		sem->count--;
	else
		result = sleepInQueue(sem, TASK_KILLABLE, MAX_SCHEDULE_TIMEOUT);

	addInActiveQueue(current, sem);
	updatePrioOfActives(sem);

	spin_unlock_irqrestore(&sem->lock, flags);

	return result;
}

static int __sched sleepInQueue(struct asasem *sem, long state, long timeout)
{
	struct task_struct *task = current;
	struct asasem_waiter waiter;

	list_add_tail(&waiter.list, &sem->wait_list);
	waiter.task = task;
	waiter.up = 0;

	updatePrioOfActives(sem);

	for (;;) {
		if (signal_pending_state(state, task))
			goto interrupted;
		if (timeout <= 0)
			goto timed_out;
		__set_task_state(task, state);
		spin_unlock_irq(&sem->lock);
		timeout = schedule_timeout(timeout);
		spin_lock_irq(&sem->lock);
		if (waiter.up)
			return 0;
	}

timed_out:
	list_del(&waiter.list);
	return -ETIME;

interrupted:
	list_del(&waiter.list);
	return -EINTR;
}

asmlinkage long sys_asasem_signal(void *asasem_voidptr)
{
	unsigned long flags;
	long result;
	struct asasem* sem=(struct asasem *)asasem_voidptr;
	result=0;

	spin_lock_irqsave(&sem->lock, flags);

	removeFromActiveQueue(current, sem);

	if (likely(list_empty(&sem->wait_list)))
		sem->count++;
	else
		wakeUpNext(sem);
	spin_unlock_irqrestore(&sem->lock, flags);

	return result;
}

static int __sched wakeUpNext(struct asasem *sem)
{
	struct asasem_waiter *hwaiter;

	hwaiter = mostPriorWaiterInWaitList(&sem->wait_list);
	list_del(&hwaiter->list);
	hwaiter->up = 1;
	wake_up_process(hwaiter->task);
	return 0;
}

static inline struct asasem_waiter* mostPriorWaiterInWaitList(struct list_head *lh)
{
	struct asasem_waiter *result, *tmp;
	struct list_head *currNode;

	result=list_first_entry(lh, struct asasem_waiter, list);

	list_for_each(currNode, lh)
	{
		tmp=list_entry(currNode, struct asasem_waiter, list);
		if (higherPrio(tmp->task , result->task))
			result=tmp;
	}

	return result;
}

static inline int addInActiveQueue(struct task_struct *p, struct asasem *sem)
{
	struct asasem_active entering=__ASASEM_ACTIVE_INITIALIZER(p, PriorityOfTaskPtr(p));
	(sem->active_arr)[numOfActiveProcs(sem)-1]=entering;
	return 0;
}

static inline int removeFromActiveQueue(struct task_struct *p, struct asasem *sem)
{
	int idx=0;
	if(numOfActiveProcs(sem)==0)
		return 1;
	if(numOfActiveProcs(sem)==1)
		return 2;
	idx=active_IdxByTaskAdr(p, sem);
	if(idx==-1)
		return -1;
	(sem->active_arr)[idx]=(sem->active_arr)[numOfActiveProcs(sem)-1];
	return 0;
}

static inline int active_IdxByTaskAdr(struct task_struct *p, struct asasem *sem)
{
	int i=0;
	for(i=0; i<numOfActiveProcs(sem); ++i)
		if(((sem->active_arr)[i]).task==p)
			return i;
	return -1;
}

static inline int updatePrioOfActives(struct asasem *sem)
{
	struct list_head *wl;
	int i=0;
	wl=&sem->wait_list;
	if(list_empty(wl))
		for(i=0; i<numOfActiveProcs(sem); ++i)
			PriorityOfTaskPtr(((sem->active_arr)[i]).task)=(((sem->active_arr)[i]).original_priority);
	else
	{
		struct task_struct *highestPrioWaitingTask=0;
		highestPrioWaitingTask=(mostPriorWaiterInWaitList(wl))->task;
		for(i=0; i<numOfActiveProcs(sem); ++i)
			if(higherPrio( highestPrioWaitingTask, ((sem->active_arr)[i]).task ) )
				PriorityOfTaskPtr(((sem->active_arr)[i]).task) = PriorityOfTaskPtr(highestPrioWaitingTask);
	}
	return 0;
}

static inline int printTaskInfo(struct task_struct *p)
{
	return printk("task address: %ld, prio: %d\n", (long)p, PriorityOfTaskPtr(p));
}

static inline int printActiveList(struct asasem *sem, int size)
{
	int i;
	printk("printing active list:\n");
	for(i=0; i<size; ++i)
		printk("task adr: %ld, prio: %d, orig_prio: %d\n",
			(long)((sem->active_arr)[i]).task, PriorityOfTaskPtr(((sem->active_arr)[i]).task), ((sem->active_arr)[i]).original_priority);
	printk("\n\n");
	return 0;
}

static inline int higherPrio(struct task_struct *p1, struct task_struct *p2)
{
	return PriorityOfTaskPtr(p1) < PriorityOfTaskPtr(p2);
}
static inline int numOfActiveProcs(struct asasem *sem)
//CAUTION: the result is not valid if the function is called between modifying count and modifying active list.
{
	return (int)((int)sem->capacity-(int)sem->count);
}
