#ifndef __schedule_h
#define __schedule_h

typedef struct wssched_s wssched_t;

typedef struct task_s {
	unsigned    dependency_count;	// task cannot run if non-zero
	unsigned    child_count;
	struct task_s **children;

	void      (*execute) (struct task_s *task, int worker_id);
	void       *data;
} task_t;

void wssched_insert (wssched_t *sched, int count, task_t **tasks);
int wssched_worker_count (const wssched_t *sched) __attribute__ ((pure));
wssched_t *wssched_create (int num_threads);
void wssched_destroy (wssched_t *sched);
#endif//__schedule_h
