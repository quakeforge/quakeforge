#ifndef __qwaq_threading_h
#define __qwaq_threading_h

#ifndef __QFCC__

#include "QF/dstring.h"
#include "QF/ringbuffer.h"

#define QUEUE_SIZE 16
#define STRING_ID_QUEUE_SIZE 8
#define COMMAND_QUEUE_SIZE 1280

typedef struct rwcond_s {
	pthread_cond_t rcond;
	pthread_cond_t wcond;
	pthread_mutex_t mut;
} rwcond_t;

typedef struct qwaq_pipe_s {
	rwcond_t    pipe_cond;
	RING_BUFFER (int, COMMAND_QUEUE_SIZE) pipe;

	rwcond_t    string_id_cond;
	RING_BUFFER (int, STRING_ID_QUEUE_SIZE + 1) string_ids;
	dstring_t   strings[STRING_ID_QUEUE_SIZE];
} qwaq_pipe_t;

void qwaq_pipe_submit (qwaq_pipe_t *pipe, const int *data, unsigned len);
void qwaq_pipe_receive (qwaq_pipe_t *pipe, int *data, int id, unsigned len);
int qwaq_pipe_acquire_string (qwaq_pipe_t *pipe);
void qwaq_pipe_release_string (qwaq_pipe_t *pipe, int string_id);

void qwaq_init_pipe (qwaq_pipe_t *pipe);
void qwaq_init_timeout (struct timespec *timeout, int64_t time);
void qwaq_init_cond (rwcond_t *cond);

#endif

#endif//__qwaq_threading_h
