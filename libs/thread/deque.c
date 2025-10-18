// based on
// https://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#include "QF/thread/deque.h"

int64_t
circarr_size (circarr_t *array)
{
	return INT64_C (1) << array->size_log;
}

circarr_t *
circarr_new (int64_t size_log)
{
	circarr_t  *array = malloc (sizeof (circarr_t));
	array->size_log = size_log;
	array->size_mask = circarr_size (array) - 1;
	array->next = nullptr;
	array->data = malloc (circarr_size (array) * sizeof (void *));
	return array;
}

void
circarr_delete (circarr_t *array)
{
	free (array->data);
	free (array);
}

void
circarr_put (circarr_t *array, int64_t index, void *obj)
{
	array->data[index & array->size_mask] = obj;
}

void *
circarr_get (circarr_t *array, int64_t index)
{
	return array->data[index & array->size_mask];
}

circarr_t *
circarr_grow (circarr_t *oldArray, int64_t bottom, int64_t top)
{
	circarr_t  *array = circarr_new (oldArray->size_log + 1);
	array->next = oldArray;
	for (int64_t i = top; i < bottom; i++) {
		circarr_put (array, i, circarr_get (oldArray, i));
	}
	return array;
}

circarr_t *
circarr_shrink (circarr_t *oldArray, int64_t bottom, int64_t top)
{
	if (!oldArray->next) {
		return oldArray;
	}

	circarr_t  *array = oldArray;
	while (oldArray->next && circarr_size (oldArray->next) > bottom - top) {
		oldArray = oldArray->next;
	}
	for (int64_t i = top; i < bottom; i++) {
		circarr_put (oldArray, i, circarr_get (array, i));
	}
	while (array != oldArray) {
		circarr_t  *t = array->next;
		circarr_delete (array);
		array = t;
	}
	return oldArray;
}

deque_t *
deque_new (int64_t size_log)
{
	deque_t    *deque = malloc (sizeof (deque_t));
	*deque = (deque_t) {
		.activeArrray = circarr_new (size_log),
	};
	return deque;
}

void
deque_delete (deque_t *deque)
{
	while (deque->activeArrray) {
		// FIXME should this be in circarr_delete?
		circarr_t  *array = deque->activeArrray->next;
		circarr_delete (deque->activeArrray);
		deque->activeArrray = array;
	}
	free (deque);
}

void
deque_pushBottom (deque_t *deque, void *obj)
{
	int64_t     bottom = atomic_load (&deque->bottom);
	int64_t     top = atomic_load (&deque->top);
	circarr_t  *array = atomic_load (&deque->activeArrray);
	int64_t     size = bottom - top;
	if (size >= circarr_size (array) - 1) {
		array = circarr_grow (array, bottom, top);
		atomic_store (&deque->activeArrray, array);
	}
	circarr_put (array, bottom, obj);
	atomic_store (&deque->bottom, bottom + 1);
}

static int
cas_top (deque_t *deque, int64_t old, int64_t new)
{
	return atomic_compare_exchange_weak (&deque->top, &old, new);
}

void *
deque_steal (deque_t *deque)
{
	int64_t     top = atomic_load (&deque->top);
	int64_t     bottom = atomic_load (&deque->bottom);
	circarr_t  *array = atomic_load (&deque->activeArrray);
	int64_t     size = bottom - top;
	if (size <= 0) {
		return nullptr;
	}
	void      *obj = circarr_get (array, top);
	if (!cas_top (deque, top, top + 1)) {
		return DEQUE_ABORT;
	}
	return obj;
}

void *
deque_popBottom (deque_t *deque)
{
	int64_t     bottom = atomic_load (&deque->bottom);
	circarr_t  *array = atomic_load (&deque->activeArrray);
	bottom = bottom - 1;
	atomic_store (&deque->bottom, bottom);
	int64_t     top = atomic_load (&deque->top);
	long        size = bottom - top;
	if (size < 0) {
		atomic_store (&deque->bottom, top);
		return nullptr;
	}
	void       *obj = circarr_get (array, bottom);
	if (size > 0) {
		return obj;
	}
	if (!cas_top (deque, top, top + 1)) {
		obj = nullptr;
	}
	atomic_store (&deque->bottom, top + 1);
	return obj;
}

int
deque_empty (deque_t *deque)
{
	int64_t     top = atomic_load (&deque->top);
	int64_t     bottom = atomic_load (&deque->bottom);
	int64_t     size = bottom - top;
	return size <= 0;
}
