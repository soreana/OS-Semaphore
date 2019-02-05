#include <linux/syscalls.h>
#include <linux/sched.h>

int print_info(struct task_struct *t)
{
	char *state_desc;
	if(t->state==0)
		state_desc="runnable";
	else if(t->state>0)
		state_desc="stopped";
	else state_desc="unrunnable";
	return printk("process id:\t%d\npriority:\t%d\nstate:\t%s(%ld)\nparent pid:\t%d\n\n",
		t->pid,t->prio,state_desc,t->state,t->parent->pid);
}

asmlinkage long sys_get_status_parents(unsigned int depth)
{
	int i=0;
	struct task_struct *t=current;
	printk("##################################################\nget_status_parents called, depth=%d\n\n",depth);
	for (t=current, i=0; t!=&init_task && i<depth; t=t->parent, i=i+1)
		print_info(t);
	print_info(t); //for the last task of the sequence (e.g. init_task and depth=0)
	return 0;
}

long dfs(struct task_struct *t)
{
	struct list_head *child_list;
	print_info(t);
	list_for_each(child_list, &t->children)
	{
		dfs(list_entry(child_list, struct task_struct, sibling));
	}
	return 0;
}

asmlinkage long sys_get_status_all(void)
{
	printk("##################################################\nget_status_all called\n\n");
	dfs(&init_task);
	return 0;
}
