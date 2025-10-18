#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#include "QF/thread/schedule.h"

task_t *tasks;
task_t **children;

static void
test_exec (task_t *task)
{
	//printf ("task: %7"PRIi64"\n", (int64_t) (task - tasks));
	for (int i = 0; i < 1<<8; i++) {
	}
	fflush (stdout);
	task->data = (void *) 1;
}

static int tests_completed = 0;
static pthread_mutex_t fence_mut = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fence_cond = PTHREAD_COND_INITIALIZER;

#define task_count ((1 << 4) - 1)

static void
test_done (task_t *task)
{
	printf ("final task: %7"PRIi64"\n", (int64_t) (task - tasks));
	task->data = (void *) 1;

	pthread_mutex_lock (&fence_mut);
	tests_completed = 1;
	pthread_cond_signal (&fence_cond);
	pthread_mutex_unlock (&fence_mut);
	printf ("final task exit\n");
}

static void
init_timeout (struct timespec *timeout, int64_t time)
{
	#define SEC 1000000000L
	struct timeval now;
	gettimeofday(&now, 0);
	timeout->tv_sec = now.tv_sec;
	timeout->tv_nsec = now.tv_usec * 1000L + time;
	if (timeout->tv_nsec >= SEC) {
		timeout->tv_sec += timeout->tv_nsec / SEC;
		timeout->tv_nsec %= SEC;
	}
}

static void
wait_complete (void)
{
	struct timespec timeout;
	init_timeout (&timeout, 50 * 1000000);
	printf ("wait_complete: wait\n");
	pthread_mutex_lock (&fence_mut);
	while (!tests_completed) {
		pthread_cond_timedwait (&fence_cond, &fence_mut, &timeout);
		//pthread_cond_wait (&fence_cond, &fence_mut);
		int dep_count = 0;
		int run_count = 0;
		for (int i = 0; i < task_count + 1; i++) {
			dep_count += tasks[i].dependency_count > 0;
			run_count += tasks[i].data != 0;
		}
		printf ("              waiting on %7d tasks, ran %7d task\r",
				dep_count, run_count);
		fflush(stdout);
		init_timeout (&timeout, 50 * 1000000);
	}
	pthread_mutex_unlock (&fence_mut);
	printf ("wait_complete: done\n");
}

static void __attribute__((used))
list_tasks (void)
{
	for (int i = 0; i < task_count + 1; i++) {
		if (!tasks[i].data) {
			printf ("task %d\n", i);
		}
	}
}

static void
dump_dag (void)
{
	return;
	FILE       *out = fopen ("dag.dot", "wt");
	fprintf (out, "digraph dag_%p {\n", tasks);
	fprintf (out, "  graph [label=\"%s\"];\n", "tasks");
	fprintf (out, "  layout=dot;\n");
	fprintf (out, "  clusterrank=local;\n");
	fprintf (out, "  rankdir=TB;\n");
	fprintf (out, "  compound=true;\n");
	for (int i = 0; i < task_count + 1; i++) {
		fprintf (out, "  \"t_%p\" [label=\"task %d\\n%s %d\"];\n", &tasks[i], i,
				 tasks[i].execute == test_exec ? "exec" : "done",
				 tasks[i].child_count);
		for (unsigned j = 0; j < tasks[i].child_count; j++) {
			fprintf (out, "  \"t_%p\" -> \"t_%p\";\n", &tasks[i],
					 tasks[i].children[j]);
		}
	}
	fprintf (out, "}\n");
	fclose (out);
}

static void
run_test_tasks (void)
{
	tests_completed = 0;
	tasks = calloc (task_count + 1, sizeof (task_t));
	children = calloc (task_count, sizeof (task_t *));
	int tind = 0;
	int cind = 1;
	for (; tind < task_count; tind++) {
		tasks[tind].execute = test_exec;
		if (cind < task_count) {
			tasks[tind].child_count = 2;
			tasks[tind].children = children + cind - 1;

			children[cind - 1] = &tasks[cind];
			tasks[cind].dependency_count = 1;

			children[cind] = &tasks[cind + 1];
			tasks[cind + 1].dependency_count = 1;

			cind += 2;
		} else {
			tasks[tind].child_count = 1;
			tasks[tind].children = children + cind - 1;
			tasks[cind].dependency_count++;
		}
	}
	int err = 0;
	for (int i = 1; i < task_count / 2; i++) {
		if (tasks[i].dependency_count != 1 || tasks[i].child_count != 2) {
			printf ("task %d has wrong counts: %d %d\n", i,
					tasks[i].dependency_count, tasks[i].child_count);
			err = 1;
		}
	}
	for (int i = task_count / 2 + 1; i < task_count; i++) {
		if (tasks[i].dependency_count != 1 || tasks[i].child_count != 1) {
			printf ("task %d has wrong counts: %d %d\n", i,
					tasks[i].dependency_count, tasks[i].child_count);
			err = 1;
		}
	}
	for (int i = task_count; i < task_count + 1; i++) {
		if (tasks[i].dependency_count != task_count - task_count / 2
			|| tasks[i].child_count != 0) {
			printf ("task %d has wrong counts: %d %d\n", i,
					tasks[i].dependency_count, tasks[i].child_count);
			err = 1;
		}
	}
	if (err) {
		exit (1);
	}
	tasks[tind].execute = test_done;
	children[cind - 1] = &tasks[tind];
	dump_dag ();
	wssched_t  *sched = wssched_create (6);
	task_t     *root_tasks[] = { &tasks[0] };
	wssched_insert (sched, sizeof (root_tasks) / sizeof (root_tasks[0]),
					root_tasks);
	wait_complete ();
	wssched_destroy (sched);
	free (tasks);
	free (children);
}

int
main (void)
{
	do { run_test_tasks (); } while (1);
	return 0;
}
