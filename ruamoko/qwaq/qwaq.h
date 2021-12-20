#ifndef __qwaq_h
#define __qwaq_h

#include <pthread.h>

#include "QF/darray.h"
#include "QF/progs.h"
#include "QF/sys.h"

typedef void (*progsinit_f) (progs_t *pr);

typedef struct qwaq_thread_s {
	pthread_t   thread_id;
	int         return_code;
	struct DARRAY_TYPE (const char *) args;
	sys_printf_t sys_printf;
	progsinit_f*progsinit;
	progs_t    *pr;
	struct hashlink_s *hashlink_freelist;
	func_t      main_func;
	void       *data;
} qwaq_thread_t;

typedef struct qwaq_thread_set_s DARRAY_TYPE(qwaq_thread_t *) qwaq_thread_set_t;

struct memhunk_s;
void BI_Init (struct memhunk_s *hunk, progs_t *pr);
void BI_Curses_Init (progs_t *pr);
void BI_TermInput_Init (progs_t *pr);
void QWAQ_EditBuffer_Init (progs_t *pr);
extern struct cbuf_s *qwaq_cbuf;
qwaq_thread_t *create_thread (void *(*thread_func) (qwaq_thread_t *), void *);

int qwaq_init_threads (qwaq_thread_set_t *thread_data);

#endif//__qwaq_h
