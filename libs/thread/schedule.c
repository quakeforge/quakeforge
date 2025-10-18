// An Efficient Work-Stealing Scheduler for Task Dependency Graph
// Chun-Xun Lin, Tsung-Wei Huang, Martin D. F. Wong
// https://tsung-wei-huang.github.io/papers/icpads20.pdf
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sched.h>
#include <stdio.h>
#include "QF/thread/deque.h"
#include "QF/thread/notifier.h"
#include "QF/thread/schedule.h"

#define MAX_STEAL 5
#define MAX_YIELD 3
#define YIELD() sched_yield()

#ifdef _WIN32
#define aligned_alloc(al, sz) _aligned_malloc(sz, al)
#define free(x) _aligned_free(x)
#endif

typedef struct worker_s {
	deque_t    *queue;
	waiter_t   *waiter;
	struct wssched_s *sched;
	pthread_t   thread_id;
} worker_t;

struct wssched_s {
	deque_t     *main_queue;
	deque_t    **worker_queues;
	worker_t    *workers;
	waiter_t    *waiters;
	notifier_t  *notifier;
	int          worker_count;
	bool         running;

	_Atomic int  num_actives;
	_Atomic int  num_thieves;
	_Atomic int  next_index;
} __attribute__((aligned(128)));

#define atomic_inc(PTR) ({atomic_fetch_add (PTR, 1) + 1;})
#define atomic_dec(PTR) ({atomic_fetch_sub (PTR, 1) - 1;})

static void
wssched_task_complete (worker_t *worker, task_t *task)
{
	task_t     *ready_tasks[task->child_count];
	task_t    **ready = ready_tasks;
	for (unsigned i = 0; i < task->child_count; i++) {
		task_t     *t = task->children[i];
		if (atomic_dec (&t->dependency_count) == 0) {
			*ready++ = t;
		}
	}
	if (ready - ready_tasks) {
		unsigned    count = ready - ready_tasks;
		for (unsigned i = 0; i < count; i++) {
			deque_pushBottom (worker->queue, ready_tasks[i]);
		}
		notifier_notify_one (worker->sched->notifier);
	}
}

void
wssched_insert (wssched_t *sched, int count, task_t **tasks)
{
	static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock (&mut);
	// lock
	for (int i = 0; i < count; i++) {
		deque_pushBottom (sched->main_queue, tasks[i]);
	}
	pthread_mutex_unlock (&mut);
	// unlock
	notifier_notify_one (sched->notifier);
}

static void
wssched_exploit_task (wssched_t *sched, task_t *task, worker_t *worker)
{
	if (task) {
		if (atomic_inc (&sched->num_actives) == 1
			&& !atomic_load (&sched->num_thieves)) {
			notifier_notify_one (sched->notifier);
		}
		do {
			task->execute (task);
			wssched_task_complete (worker, task);
			task = deque_popBottom (worker->queue);
		} while (task);
		atomic_dec (&sched->num_actives);
	}
}

static int
wssched_index (wssched_t *sched)
{
	int         new_index;
	int         index = atomic_load (&sched->next_index);
	do {
		new_index = index + 1;
		if (new_index >= sched->worker_count) {
			new_index = 0;
		}
	} while (!atomic_compare_exchange_weak (&sched->next_index,
											&index, new_index));
	return new_index;
}

static task_t *
wssched_explore_task (wssched_t *sched, worker_t *worker)
{
	int         num_failed_steals = 0;
	int         num_yields = 0;
	task_t     *task = nullptr;
	while (atomic_load (&sched->running)) {
		worker_t   *victim = &sched->workers[wssched_index (sched)];
		if (victim == worker) {
			task = deque_steal (sched->main_queue);
		} else {
			task = deque_steal (victim->queue);
		}
		if (task == DEQUE_ABORT) {
			continue;
		}
		if (task) {
			break;
		} else {
			num_failed_steals++;
			if (num_failed_steals >= MAX_STEAL) {
				YIELD ();
				num_yields++;
				if (num_yields == MAX_YIELD) {
					break;
				}
			}
		}
	}
	return task;
}

static bool
wssched_wait_for_task (wssched_t *sched, task_t **task, worker_t *worker)
{
wait_for_task:
	atomic_inc (&sched->num_thieves);
explore_task:
	if ((*task = wssched_explore_task (sched, worker))) {
		if (atomic_dec (&sched->num_thieves) == 0) {
			notifier_notify_one (sched->notifier);
		}
		return true;
	}
	notifier_prepare_to_wait (sched->notifier, worker->waiter);
	if (!deque_empty (sched->main_queue)) {
		notifier_cancel_wait (sched->notifier, worker->waiter);
		*task = deque_steal (sched->main_queue);
		if (*task && *task != DEQUE_ABORT) {
			if (atomic_dec (&sched->num_thieves) == 0) {
				notifier_notify_one (sched->notifier);
			}
			return true;
		} else {
			goto explore_task;
		}
	}
	if (!atomic_load (&sched->running)) {
		notifier_cancel_wait (sched->notifier, worker->waiter);
		notifier_notify_all (sched->notifier);
		atomic_dec (&sched->num_thieves);
		return false;
	}
	if (atomic_dec (&sched->num_thieves) == 0
		&& atomic_load (&sched->num_actives)) {
		notifier_cancel_wait (sched->notifier, worker->waiter);
		goto wait_for_task;
	}
	notifier_commit_wait (sched->notifier, worker->waiter);
	return true;
}

static void
wssched_control_flow (worker_t *worker )
{
	wssched_t  *sched = worker->sched;
	task_t     *task = nullptr;
	do {
		wssched_exploit_task (sched, task, worker);
	} while (wssched_wait_for_task (sched, &task, worker));
}

static void *
run_worker (void *data)
{
	worker_t   *worker = data;
	wssched_control_flow (worker);
	return nullptr;
}

wssched_t *
wssched_create (int num_threads)
{
	size_t size = sizeof (wssched_t)
				+ sizeof (waiter_t[num_threads])
				+ sizeof (deque_t *[num_threads])
				+ sizeof (worker_t[num_threads]);
	wssched_t  *sched = aligned_alloc (128, size);
	*sched = (wssched_t) {
		.main_queue = deque_new (DEQUE_DEF_SIZE),
		.worker_count = num_threads,
		.running = true,
	};
	sched->waiters = (waiter_t *) &sched[1];
	sched->worker_queues = (deque_t **) &sched->waiters[num_threads];
	sched->workers = (worker_t *) &sched->worker_queues[num_threads];
	sched->notifier = notifier_new (num_threads, sched->waiters);

	for (int i = 0; i < num_threads; i++) {
		sched->worker_queues[i] = deque_new (DEQUE_DEF_SIZE);
		sched->workers[i] = (worker_t) {
			.queue = sched->worker_queues[i],
			.waiter = &sched->waiters[i],
			.sched = sched,
		};
		sched->waiters[i] = (waiter_t) {
			.mut = PTHREAD_MUTEX_INITIALIZER,
			.cond = PTHREAD_COND_INITIALIZER,
		};
	}
	for (int i = 0; i < num_threads; i++) {
		worker_t   *worker = &sched->workers[i];
		pthread_create (&worker->thread_id, nullptr, run_worker, worker);
	}
	return sched;
}

void
wssched_destroy (wssched_t *sched)
{
	atomic_store (&sched->running, false);
	notifier_notify_all (sched->notifier);
	for (int i = 0; i < sched->worker_count; i++) {
		worker_t   *worker = &sched->workers[i];
		pthread_join (worker->thread_id, nullptr);
	}
	notifier_delete (sched->notifier);
	for (int i = 0; i < sched->worker_count; i++) {
		deque_delete (sched->worker_queues[i]);
	}
	deque_delete (sched->main_queue);
	free (sched);
}
