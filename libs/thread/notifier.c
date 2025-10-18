// Based on the notifier class in the Eigen library.
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include "QF/thread/notifier.h"

#define YIELD() sched_yield()

#define WAITER_BITS 14
#define STACK_MASK ((UINT64_C(1) << WAITER_BITS) - 1)
#define WAITER_SHIFT WAITER_BITS
#define WAITER_MASK (((UINT64_C(1) << WAITER_BITS) - 1) << WAITER_SHIFT)
#define WAITER_INC (UINT64_C(1) << WAITER_SHIFT)
#define SIGNAL_SHIFT (2 * WAITER_BITS)
#define SIGNAL_MASK (((UINT64_C(1) << WAITER_BITS) - 1) << SIGNAL_SHIFT)
#define SIGNAL_INC (UINT64_C(1) << SIGNAL_SHIFT)
#define EPOCH_SHIFT (3 * WAITER_BITS)
#define EPOCH_BITS (64 - EPOCH_SHIFT)
#define EPOCH_MASK (((UINT64_C(1) << EPOCH_BITS) - 1) << EPOCH_SHIFT)
#define EPOCH_INC (UINT64_C(1) << EPOCH_SHIFT)

notifier_t *
notifier_new (int max_waiters, waiter_t *waiters)
{
	notifier_t *notifier = malloc (sizeof (notifier_t));
	*notifier = (notifier_t) {
		.state = STACK_MASK,
		.waiters = waiters,
	};
	return notifier;
}

void
notifier_delete (notifier_t *notifier)
{
	free (notifier);
}

static void
notifier_park (notifier_t *notifier, waiter_t *waiter)
{
	pthread_mutex_lock (&waiter->mut);
	while (atomic_load (&waiter->state) != wsSignaled) {
		atomic_store (&waiter->state, wsWaiting);
		pthread_cond_wait (&waiter->cond, &waiter->mut);
	}
	pthread_mutex_unlock (&waiter->mut);
}

static void
notifier_unpark (notifier_t *notifier, waiter_t *waiter)
{
	for (waiter_t *next; waiter; waiter = next) {
		uint64_t    wnext = atomic_load_explicit (&waiter->next,
												  memory_order_relaxed)
							& STACK_MASK;
		next = (wnext == STACK_MASK) ? nullptr : &notifier->waiters[wnext];
		waiter_state state;

		pthread_mutex_lock (&waiter->mut);
		state = atomic_load (&waiter->state);
		atomic_store (&waiter->state, wsSignaled);
		if (state == wsWaiting) {
			// notifiy only if it was waiting
			pthread_cond_signal (&waiter->cond);
		}
		pthread_mutex_unlock (&waiter->mut);
	}
}

void
notifier_prepare_to_wait (notifier_t *notifier, waiter_t *waiter)
{
	uint64_t    state = atomic_load_explicit (&notifier->state,
											  memory_order_relaxed);
	for (;;) {
		uint64_t    newstate = state + WAITER_INC;
		if (atomic_compare_exchange_weak_explicit (&notifier->state,
												   &state, newstate,
												   memory_order_seq_cst,
												   memory_order_seq_cst)) {
			return;
		}
	}
}

void
notifier_commit_wait (notifier_t *notifier, waiter_t *waiter)
{
	waiter->state = wsNotSignaled;
	const uint64_t me = (waiter - notifier->waiters) | waiter->epoch;
	uint64_t    state = atomic_load_explicit (&notifier->state,
											  memory_order_seq_cst);
	while (1) {
		uint64_t    newstate;
		if (state & SIGNAL_MASK) {
			// consume the signal and return immediately
			newstate = state - WAITER_INC - SIGNAL_INC;
		} else {
			// remove this thread from the pre-wait counter and add to the
			// waiter stack
			newstate = ((state & WAITER_MASK) - WAITER_INC) | me;
			atomic_store_explicit (&waiter->next,
								   state & (STACK_MASK | EPOCH_MASK),
								   memory_order_relaxed);
		}
		if (atomic_compare_exchange_weak_explicit (&notifier->state,
												   &state, newstate,
												   memory_order_release,
												   memory_order_relaxed)) {
			if (!(state & SIGNAL_MASK)) {
				waiter->epoch += EPOCH_INC;
				notifier_park (notifier, waiter);
			}
			return;
		}
	}
}

void
notifier_cancel_wait (notifier_t *notifier, waiter_t *waiter)
{
	uint64_t    state = atomic_load (&notifier->state);
	while (1) {
		uint64_t    newstate = state - WAITER_INC;
		// We don't know if the thread was also notified or not, so we
		// should not consume a signal unconditionally. Only if the number of
		// waiters is equal to the number of signals do we know that
		// the thread was notified and we must take away the signal
		if (((state & WAITER_MASK) >> WAITER_SHIFT)
			== ((state & SIGNAL_MASK) >> SIGNAL_SHIFT)) {
			newstate -= SIGNAL_INC;
		}
		if (atomic_compare_exchange_weak_explicit (&notifier->state,
												   &state, newstate,
												   memory_order_acq_rel,
												   memory_order_acquire)) {
			return;
		}
	}
}

void
notifier_notify (notifier_t *notifier, int notifyAll)
{
	atomic_thread_fence (memory_order_seq_cst);
	uint64_t    state = atomic_load (&notifier->state);
	while (1) {
		uint64_t    waiters = (state & WAITER_MASK) >> WAITER_SHIFT;
		uint64_t    signals = (state & SIGNAL_MASK) >> SIGNAL_SHIFT;
		// easy case: no waiters
		if ((state & STACK_MASK) == STACK_MASK && waiters == signals) {
			return;
		}
		uint64_t    newstate;
		if (notifyAll) {
			// Empty wait stack and set signal to number of pre-wait threads.
			newstate = (state & WAITER_MASK)
					   | (waiters << SIGNAL_SHIFT)
					   | STACK_MASK;
		} else if (signals < waiters) {
			// There is a thread in pre-wait state, unblock it.
			newstate = state + SIGNAL_INC;
		} else {
			// Pop a waiter from list and unpark it.
			waiter_t   *waiter = &notifier->waiters[state & STACK_MASK];
			uint64_t    next = atomic_load_explicit (&waiter->next,
													 memory_order_relaxed);
			newstate = (state & (WAITER_MASK | SIGNAL_MASK)) | next;
		}
		if (atomic_compare_exchange_weak_explicit (&notifier->state,
												   &state, newstate,
												   memory_order_acq_rel,
												   memory_order_acquire)) {
			if (!notifyAll && (signals < waiters)) {
				return;	// unblocked pre-wait thread
			}
			if ((state & STACK_MASK) == STACK_MASK) {
				return;
			}
			waiter_t   *waiter = &notifier->waiters[state & STACK_MASK];
			if (!notifyAll) {
				atomic_store_explicit (&waiter->next, STACK_MASK,
									   memory_order_relaxed);
			}
			notifier_unpark (notifier, waiter);
			return;
		}
	}
}
