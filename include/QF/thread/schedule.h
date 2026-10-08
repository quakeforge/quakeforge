#ifndef __schedule_h
#define __schedule_h

typedef struct wssched_s wssched_t;

typedef struct task_s {
	unsigned    dependency_count;	// task cannot run if non-zero
	// List of tasks that depend on this task. Do NOT insert child
	// tasks: they will be inserted automatically when their
	// dependency_count hits 0.
	unsigned    child_count;
	struct task_s **children;

	// task is a pointer to this task_t, worker_id is the thread id
	// (0-num_threads-1) and can be used to index arrays of per-thread
	// resources.
	void      (*execute) (struct task_s *task, int worker_id);
	// Data pointer for use by the execute function, not touched by the
	// scheduler. Note that the concept of a task id does not apply to
	// the scheduler, it must somehow be fetched from data. That mechanism
	// is up to the user. On way is to have an array of task_t that contains
	// this task, reference the array somehow via data, and subtract the task
	// pointer from that array.
	void       *data;
} task_t;

// tasks is an array of pointers to root tasks to be scheduled. Do NOT insert
// any child tasks (dependency_count > 0): they will be scheduled automatically
// when all their parent tasks (which have pointers to the child tasks) are
// complete (dependency_count reaches 0).
void wssched_insert (wssched_t *sched, int count, task_t **tasks);

// The number of threads used by the scheduler. worker_id (passed to
// thread.execute()) is always in the range 0..num_threads-1. Useful for
// setting up per-thread resources for scheduled tasks.
int wssched_worker_count (const wssched_t *sched) __attribute__ ((pure));

wssched_t *wssched_create (int num_threads);
void wssched_destroy (wssched_t *sched);
#endif//__schedule_h
