#ifndef __qwaq_h
#define __qwaq_h

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

void BI_Init (progs_t *pr);
void QWAQ_EditBuffer_Init (progs_t *pr);
extern struct cbuf_s *qwaq_cbuf;
qwaq_thread_t *create_thread (void *(*thread_func) (qwaq_thread_t *), void *);

#endif//__qwaq_h
