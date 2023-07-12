#include "ruamoko/qwaq/debugger/debug.h"

void traceon() = #0;
void traceoff() = #0;

void qdb_set_trace (qdb_target_t target, int state) = #0;
int qdb_set_breakpoint (qdb_target_t target, unsigned staddr) = #0;
int qdb_clear_breakpoint (qdb_target_t target, unsigned staddr) = #0;
int qdb_set_watchpoint (qdb_target_t target, unsigned offset) = #0;
int qdb_clear_watchpoint (qdb_target_t target) = #0;
int qdb_continue (qdb_target_t target) = #0;
qdb_state_t qdb_get_state (qdb_target_t target) = #0;
int qdb_get_stack_depth (qdb_target_t target) = #0;
qdb_stack_t *qdb_get_stack (qdb_target_t target) = #0;
int qdb_get_event (qdb_target_t target, qdb_event_t *event) = #0;
int qdb_get_data (qdb_target_t target, unsigned src, unsigned len,
				  void *dst) = #0;
string qdb_get_string (qdb_target_t target, unsigned str) = #0;
string qdb_get_string (qdb_target_t target, string str) = #0;
string qdb_get_file_path (qdb_target_t target, string file) = #0;
int qdb_find_string (qdb_target_t target, string str) = #0;
qdb_def_t qdb_find_global (qdb_target_t target, string name) = #0;
qdb_def_t qdb_find_field (qdb_target_t target, string name) = #0;
qdb_function_t *qdb_find_function (qdb_target_t target, string name) = #0;
qdb_function_t *qdb_get_function (qdb_target_t target, unsigned fnum) = #0;
qdb_auxfunction_t *qdb_find_auxfunction (qdb_target_t target,
										 string name) = #0;
qdb_auxfunction_t *qdb_get_auxfunction (qdb_target_t target,
										unsigned fnum) = #0;
qdb_def_t *qdb_get_local_defs (qdb_target_t target, unsigned fnum) = #0;
unsigned qdb_get_source_line_addr(qdb_target_t target, string file,
								  unsigned line) = #0;
int qdb_has_data_stack (qdb_target_t target) = #0;
unsigned qdb_get_frame_addr (qdb_target_t target) = #0;
