#ifndef __qwaq_h
#define __qwaq_h

#include "QF/darray.h"

typedef struct qwaq_thread_s {
	pthread_t   thread_id;
	int         return_code;
	struct DARRAY_TYPE (const char *) args;
	sys_printf_t sys_printf;
	progs_t    *pr;
	func_t      main_func;
} qwaq_thread_t;

void BI_Init (progs_t *pr);
extern struct cbuf_s *qwaq_cbuf;
qwaq_thread_t *create_thread (void *(*thread_func) (void *));

#endif//__qwaq_h
