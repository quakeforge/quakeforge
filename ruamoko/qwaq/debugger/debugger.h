#ifndef __qwaq_debugger_debugger_h
#define __qwaq_debugger_debugger_h

#include <Object.h>

#include "ruamoko/qwaq/debugger/debug.h"
#include "ruamoko/qwaq/debugger/localsdata.h"

@class Debugger;
@protocol DebugFile
-gotoLine:(int)line;
-highlightLine;
-(string)filename;
-(ivec2)cursor;
-setDebugger:(Debugger *)debugger;
@end

@protocol DebugGetFile <Object>
-(id<DebugFile>)showFile:(string)filename path:(string)filepath;
@end

@interface Debugger : Object
{
	qdb_target_t target;
	qdb_event_t event;
	struct {
		qdb_state_t state;
		int         depth;
		unsigned	until_function;
	}           trace_cond;
	struct {
		int         onEnter;
		int         onExit;
	}           sub_cond;
	SEL         traceHandler;
	SEL         breakHandler;
	int         running;

	LocalsData *locals_data;
	id<Table>   locals_view;
	id<DebugGetFile> file_manager;
}
+(Debugger *)withTarget:(qdb_target_t)target
			fileManager:(id<DebugGetFile>) fileManager;
-(qdb_target_t)target;
-handleDebugEvent;

-run:(id<DebugFile>)file;
-gotoCursor:(id<DebugFile>)file;
-traceInto:(id<DebugFile>)file;
-stepOver:(id<DebugFile>)file;
@end

#endif//__qwaq_debugger_debugger_h
