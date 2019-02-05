#include <stdio.h>
#include <unistd.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

void* asasem;

int main(void);
int doParent(void);
int doChild(void);
int doChild_afterFork(void);
int doGrandChild(void);
int asaBusyWait_sec(double w);

int main (void)
{
	pid_t child;
	asasem = malloc(1000);
	if(!asasem)
	{
		perror("error malloc\n\n") ;
		exit(0);
	}
	if (syscall(__NR_asasem_init, asasem, 1))
	{
		perror("error on creating asasem\n\n") ;
		exit(1);
	}
	child = fork() ;
	if(child>=0)
		if(child)
			doParent();
		else
			doChild();
	else
	{
		perror("error on forking child\n\n") ;
		exit(2);
	}
	return 0 ;
}

int doChild(void)
{
	pid_t grandchild;
	grandchild=fork();

	if(grandchild>=0)
		if(grandchild)
			doChild_afterFork();
		else
			doGrandChild();
	else
	{
		perror("error on forking grandchild\n\n") ;
		exit(3);
	}
	return 0;
}

/*
	prio:
	parent > child > grandChild
	5	< 10	<	15
*/

int doParent(void)
{
	if(setpriority(PRIO_PROCESS, 0, 5)==-1)
	{
		perror("error on changing priority, Parent\n\n") ;
		exit(6);
	}
	asaBusyWait_sec(2);
	printf("I am parent, waiting for the semaphore after my child\n\n");
	if(syscall (__NR_asasem_wait, asasem))
	{
		perror("error on getting asasem, Parent\n\n") ;
		exit(9);
	}
	printf("Ha ha ha, I am parent, having semaphore before my child.\nMy priority is %d\n leaving critical section after 1 second(s). \n\n", getpriority(PRIO_PROCESS, 0));
	asaBusyWait_sec(1);
	syscall (__NR_asasem_signal, asasem) ;
	return 0;
}

int doChild_afterFork(void)
{
	if(setpriority(PRIO_PROCESS, 0, 10)==-1)
	{
		perror("error on changing priority, Child\n\n") ;
		exit(6);
	}
	asaBusyWait_sec(1);
	printf("I am child, waiting for the semaphore before my parent\n\n");
	if(syscall (__NR_asasem_wait, asasem))
	{
		perror("error on getting asasem, Child\n\n") ;
		exit(8);
	}
	printf("I am poor child, the last one.\nMy priority is %d\n leaving critical section after some second(s). \n\n", getpriority(PRIO_PROCESS, 0));
	asaBusyWait_sec(1);
	syscall (__NR_asasem_signal, asasem) ;
	return 0;
}

int doGrandChild(void)
{
	if(setpriority(PRIO_PROCESS, 0, 15)==-1)
	{
		perror("error on changing priority, grandChild\n\n") ;
		exit(5);
	}
	printf("I am grandChild, I am grandChild, waiting for the semaphore before anyone else\n\n");
	if(syscall (__NR_asasem_wait, asasem))
	{
		perror("error on getting asasem for the first time, grandChild\n\n") ;
		exit(7);
	}
	printf("I am grandChild, I had just got the semaphore, I want to sleep\n\n");
	asaBusyWait_sec(1);
	printf("I am grandChild, my inherited priority is: %d\nI am leaving the critical section, 1 second(s) please.\n\n",getpriority(PRIO_PROCESS, 0));
	asaBusyWait_sec(1);
	syscall (__NR_asasem_signal, asasem) ;
	return 0;
}


int asaBusyWait_sec(double w)
{
	clock_t start, end;
	start=clock();
	end=clock();
	while( ( (double)(end-start)/CLOCKS_PER_SEC ) <w)
		end=clock();
}
