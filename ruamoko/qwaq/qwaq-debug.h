#ifndef __qwaq_debug_h
#define __qwaq_debug_h

#include "event.h"

enum {
	qe_debug_event = 0x0100,
} qwaq_debug_messages;

#ifdef __QFCC__

//FIXME add unsigned to qfcc
#ifndef unsigned
#define unsigned int
#define umax 0x7fffffff
#endif

#else//GCC
#endif

typedef struct qdb_state_s {
	unsigned    staddr;
	unsigned    func;
	unsigned    file;
	unsigned    line;
} qdb_state_t;

void QWAQ_Debug_Init (progs_t *pr);
void QWAQ_DebugTarget_Init (progs_t *pr);

#endif//__qwaq_debug_h
