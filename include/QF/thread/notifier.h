#ifndef __notifier_h
#define __notifier_h

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

typedef enum {
	wsNotSignaled,
	wsWaiting,
	wsSignaled,
} waiter_state;

typedef struct waiter_s {
	_Atomic uint64_t next;
	_Atomic uint64_t epoch;
	_Atomic waiter_state state;
	pthread_mutex_t mut;
	pthread_cond_t cond;
} __attribute__ ((aligned(128))) waiter_t;

typedef struct notifier_s {
	_Atomic uint64_t state;
	waiter_t   *waiters;	// not owned
} notifier_t;

typedef struct notifier_s notifier_t;

notifier_t *notifier_new (int max_waiters, waiter_t *waiters);
void notifier_delete (notifier_t *notifier);
void notifier_prepare_to_wait (notifier_t *notifier, waiter_t *waiter);
void notifier_commit_wait (notifier_t *notifier, waiter_t *waiter);
void notifier_cancel_wait (notifier_t *notifier, waiter_t *waiter);
void notifier_notify (notifier_t *notifier, int all);
#define notifier_notify_one(notifier) notifier_notify (notifier, 0)
#define notifier_notify_all(notifier) notifier_notify (notifier, 1)

#endif//__notifier_h
