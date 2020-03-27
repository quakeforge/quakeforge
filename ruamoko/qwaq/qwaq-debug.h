#ifndef __qwaq_debug_h
#define __qwaq_debug_h

#include "event.h"

typedef enum {
	qe_debug_event = 0x0100,
} qwaq_debug_messages;

#ifdef __QFCC__

//FIXME finish unsigned in qfcc
#ifndef umax
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

typedef struct qdb_def_s {
	unsigned    type_size;	// type in lower 16, size in upper 16
	unsigned    offset;
	unsigned    name;	// string
	unsigned    type_encoding;
} qdb_def_t;

typedef struct qdb_function_s {
	int         staddr;
	unsigned    local_data;
	unsigned    local_size;
	unsigned	profile;
	unsigned    name;	// string
	unsigned    file;	// string
	unsigned    num_params;
} qdb_function_t;

typedef struct qdb_auxfunction_s {
	unsigned    function;
	unsigned    source_line;
	unsigned    line_info;
	unsigned    local_defs;
	unsigned    num_locals;
	unsigned    return_type;
} qdb_auxfunction_t;

#ifdef __QFCC__

typedef struct qdb_target_s { int handle; } qdb_target_t;

void qdb_set_trace (qdb_target_t target, int state);
int qdb_set_breakpoint (qdb_target_t target, unsigned staddr);
int qdb_clear_breakpoint (qdb_target_t target, unsigned staddr);
int qdb_set_watchpoint (qdb_target_t target, unsigned offset);
int qdb_clear_watchpoint (qdb_target_t target);
int qdb_continue (qdb_target_t target);
qdb_state_t qdb_get_state (qdb_target_t target);
int qdb_get_data (qdb_target_t target, unsigned src, unsigned len, void *dst);
string qdb_get_string (qdb_target_t target, unsigned str);
qdb_def_t qdb_find_global (qdb_target_t target, string name);
qdb_def_t qdb_find_field (qdb_target_t target, string name);
qdb_function_t *qdb_find_function (qdb_target_t target, string name);
qdb_function_t *qdb_get_function (qdb_target_t target, unsigned fnum);
qdb_auxfunction_t *qdb_find_auxfunction (qdb_target_t target, string name);
qdb_auxfunction_t *qdb_get_auxfunction (qdb_target_t target, unsigned fnum);
qdb_def_t *qdb_get_local_defs (qdb_target_t target, unsigned fnum);

void traceon();
void traceoff();

#else//GCC

void QWAQ_Debug_Init (progs_t *pr);
void QWAQ_DebugTarget_Init (progs_t *pr);

#endif

#endif//__qwaq_debug_h
