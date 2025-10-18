#ifndef __deque_h
#define __deque_h

#include <stdint.h>

typedef struct circarr_s {
	int64_t     size_log;
	int64_t     size_mask;
	struct circarr_s *next;
	void      **data;
} circarr_t;

circarr_t *circarr_new (int64_t size_log);
void circarr_delete (circarr_t *array);
int64_t circarr_size (circarr_t *array) __attribute__((pure));
void circarr_put (circarr_t *array, int64_t index, void *obj);
void *circarr_get (circarr_t *array, int64_t index) __attribute__((pure));
circarr_t *circarr_grow (circarr_t *oldArray, int64_t bottom, int64_t top);
circarr_t *circarr_shrink (circarr_t *oldArray, int64_t bottom, int64_t top);

typedef struct deque_s {
	int64_t     bottom;
	int64_t     top;
	circarr_t  *activeArrray;
} deque_t;

#define DEQUE_ABORT ((void *) 1)
#define DEQUE_DEF_SIZE 6	// 64 entries

deque_t *deque_new (int64_t size_log);
void deque_delete (deque_t *deque);
void deque_pushBottom (deque_t *deque, void *obj);
void *deque_steal (deque_t *deque);
void *deque_popBottom (deque_t *deque);
int  deque_empty (deque_t *deque);

#endif//__deque_h
