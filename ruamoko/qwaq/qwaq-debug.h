#ifndef __qwaq_debug_h
#define __qwaq_debug_h

#include "event.h"

typedef enum {
	qe_debug_event = 0x0100,
} qwaq_debug_messages;

#ifdef __QFCC__

//FIXME add unsigned to qfcc
#ifndef unsigned
#define unsigned int
#define umax 0x7fffffff
#endif

typedef string string_t;

#endif

typedef struct qdb_state_s {
	unsigned    staddr;
	unsigned    func;
	string_t    file;
	unsigned    line;
} qdb_state_t;

#ifdef __QFCC__

typedef struct qdb_target_s { int handle; } qdb_target_t;

@extern void qdb_set_trace (qdb_target_t target, int state);
@extern int qdb_set_breakpoint (qdb_target_t target, unsigned staddr);
@extern int qdb_clear_breakpoint (qdb_target_t target, unsigned staddr);
@extern int qdb_set_watchpoint (qdb_target_t target, unsigned offset);
@extern int qdb_clear_watchpoint (qdb_target_t target);
@extern int qdb_continue (qdb_target_t target);
@extern qdb_state_t qdb_get_state (qdb_target_t target);

#else//GCC

void QWAQ_Debug_Init (progs_t *pr);
void QWAQ_DebugTarget_Init (progs_t *pr);

#endif

#endif//__qwaq_debug_h
