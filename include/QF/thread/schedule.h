#ifndef __schedule_h
#define __schedule_h

typedef struct wssched_s wssched_t;

typedef struct task_s {
	unsigned    dependency_count;	// task cannot run if non-zero
	unsigned    child_count;
	struct task_s **children;

	void      (*execute) (struct task_s *task);
	void       *data;
} task_t;

void wssched_insert (wssched_t *sched, int count, task_t **tasks);
wssched_t *wssched_create (int num_threads);
void wssched_destroy (wssched_t *sched);
#endif//__schedule_h
