#ifndef __qwaq_debugger_debugger_h
#define __qwaq_debugger_debugger_h

#include <Object.h>

#include "debugger/debug.h"

@class ProxyView;
@class Editor;
@class Window;
@class Array;

@interface Debugger : Object
{
	qdb_target_t debug_target;

	Window     *source_window;
	ProxyView  *file_proxy;
	Array      *files;
	Editor     *current_file;

	Window     *locals_window;
	View       *locals_view;

	unsigned    current_fnum;
	qdb_function_t *func;
	qdb_auxfunction_t *aux_func;
	qdb_def_t  *local_defs;
	void       *local_data;
}
-(qdb_target_t)debug_target;
-initWithTarget:(qdb_target_t) target;
-handleDebugEvent;
@end

#endif//__qwaq_debugger_debugger_h
