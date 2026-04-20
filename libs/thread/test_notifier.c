#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include "QF/thread/notifier.h"

#define YIELD() sched_yield()

#define VERIFY_EQ(x, v) do { if (x != v) { printf ("%d: %s != %s\n", __LINE__, #x, #v); exit (1); } } while (0)
#define VERIFY_LE(x, v) do { if (x > v) { printf ("%d: %s > %s\n", __LINE__, #x, #v); exit (1); } } while (0)
#define VERIFY_GE(x, v) do { if (x < v) { printf ("%d: %s < %s\n", __LINE__, #x, #v); exit (1); } } while (0)

static void
test_basic_notifier (void)
{
	waiter_t    waiter = {
		.mut = PTHREAD_MUTEX_INITIALIZER,
		.cond = PTHREAD_COND_INITIALIZER,
	};
	notifier_t *notifier = notifier_new (1, &waiter);
	notifier_notify (notifier, 0);
	notifier_prepare_to_wait (notifier, &waiter);
	notifier_notify (notifier, 1);
	notifier_commit_wait (notifier, &waiter);
	notifier_prepare_to_wait (notifier, &waiter);
	notifier_cancel_wait (notifier, &waiter);
	notifier_delete (notifier);
}

#define queue_size 10
typedef struct testqueue_s {
	_Atomic int val;
} testqueue_t;

static testqueue_t *
testqueue_new (void)
{
	testqueue_t *q = malloc (sizeof (testqueue_t));
	*q = (testqueue_t) {
		.val = 0,
	};
	return q;
}

static void
testqueue_delete (testqueue_t *q)
{
	VERIFY_EQ (atomic_load (&q->val), 0);
	free (q);
}

static int
testqueue_push (testqueue_t *q)
{
	int         val = atomic_load (&q->val);
	for (;;) {
		VERIFY_GE (val, 0);
		VERIFY_LE (val, queue_size);
		if (val == queue_size) {
			return 0;
		}
		if (atomic_compare_exchange_weak_explicit (&q->val, &val, val + 1,
												   memory_order_release,
												   memory_order_relaxed)) {
			return 1;
		}
	}
}

static int
testqueue_pop (testqueue_t *q)
{
	int         val = atomic_load (&q->val);
	for (;;) {
		VERIFY_GE (val, 0);
		VERIFY_LE (val, queue_size);
		if (val == 0) {
			return 0;
		}
		if (atomic_compare_exchange_weak_explicit (&q->val, &val, val - 1,
												   memory_order_release,
												   memory_order_relaxed)) {
			return 1;
		}
	}
}

static int
testqueue_empty (testqueue_t *q)
{
	return atomic_load (&q->val) == 0;
}

typedef struct thread_s {
	notifier_t *notifier;
	waiter_t   *waiter;
	testqueue_t **queues;
	pthread_t   id;
} thread_t;

#define num_events (1 << 20)
#define num_threads (3)
#define num_queues (10)

static void *
run_producer (void *_t)
{
	thread_t   *t = _t;
	unsigned    rnd = t->id;
	for (int j = 0; j < num_events; j++) {
		unsigned    ind = rand_r (&rnd) % num_queues;
		if (testqueue_push (t->queues[ind])) {
			notifier_notify (t->notifier, 1);
			continue;
		}
		YIELD ();
		j--;
	}
	printf ("producer finished: %"PRIi64"\n", t->id);
	return 0;
}

static void *
run_consumer (void *_t)
{
	thread_t   *t = _t;
	waiter_t   *w = t->waiter;
	unsigned    rnd = t->id;
	for (int j = 0; j < num_events; j++) {
		unsigned    ind = rand_r (&rnd) % num_queues;
		if (testqueue_pop (t->queues[ind])) {
			continue;
		}
		j--;
		notifier_prepare_to_wait (t->notifier, w);
		int         empty = 1;
		for (int q = 0; q < num_queues; q++) {
			if (!testqueue_empty (t->queues[q])) {
				empty = 0;
				break;
			}
		}
		if (!empty) {
			notifier_cancel_wait (t->notifier, w);
			continue;
		}
		notifier_commit_wait (t->notifier, w);
	}
	printf ("consumer finished: %"PRIi64"\n", t->id);
	return 0;
}

static void
test_stress_eventcount (void)
{
	waiter_t    waiters[num_threads];
	for (int i = 0; i < num_threads; i++) {
		waiters[i] = (waiter_t) {
			.mut = PTHREAD_MUTEX_INITIALIZER,
			.cond = PTHREAD_COND_INITIALIZER,
		};
	}
	notifier_t *notifier = notifier_new (num_threads, waiters);

	testqueue_t *queues[num_queues];
	for (int i = 0; i < num_queues; i++) {
		queues[i] = testqueue_new ();
	}

	thread_t    producers[num_threads];
	for (int i = 0; i < num_threads; i++) {
		producers[i] = (thread_t) {
			.notifier = notifier,
			.queues = queues,
		};
		pthread_create (&producers[i].id, 0, run_producer, &producers[i]);
	}

	thread_t    consumers[num_threads];
	for (int i = 0; i < num_threads; i++) {
		consumers[i] = (thread_t) {
			.notifier = notifier,
			.queues = queues,
			.waiter = &waiters[i],
		};
		pthread_create (&consumers[i].id, 0, run_consumer, &consumers[i]);
	}

	for (int i = 0; i < num_threads; i++) {
		pthread_join (producers[i].id, 0);
		pthread_join (consumers[i].id, 0);
	}
	for (int i = 0; i < num_queues; i++) {
		testqueue_delete (queues[i]);
	}
}

int
main (void)
{
	puts ("start");
	test_basic_notifier ();
	test_stress_eventcount ();
	puts ("stop");
	return 0;
}
