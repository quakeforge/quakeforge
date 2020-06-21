#ifndef __qwaq_debugger_debugger_h
#define __qwaq_debugger_debugger_h

#include <types.h>
#include <Object.h>

#include "ruamoko/qwaq/debugger/debug.h"
#include "ruamoko/qwaq/debugger/localsdata.h"

@class ProxyView;
@class Editor;
@class ScrollBar;
@class Window;
@class Array;
@class TableView;

@interface Debugger : Object
{
	qdb_target_t target;
	qdb_event_t event;
	struct {
		qdb_state_t state;
		int         depth;
		int			until_function;
	}           trace_cond;
	struct {
		int         onEnter;
		int         onExit;
	}           sub_cond;
	SEL         traceHandler;

	Window     *source_window;
	ScrollBar  *source_scrollbar;
	ProxyView  *file_proxy;
	Array      *files;
	Editor     *current_file;

	Window     *locals_window;
	LocalsData *locals_data;
	TableView  *locals_view;
}
+(Debugger *)withTarget:(qdb_target_t)target;
-initWithTarget:(qdb_target_t) target;
-(qdb_target_t)target;
-handleDebugEvent;
@end

#endif//__qwaq_debugger_debugger_h
