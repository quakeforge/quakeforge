#ifndef __qwaq_debugger_h
#define __qwaq_debugger_h

#include <Object.h>

#include "qwaq-debug.h"

@class ProxyView;
@class Editor;
@class Window;
@class Array;

@interface Debugger : Object
{
	Window     *source_window;
	ProxyView  *file_proxy;
	Array      *files;
	Editor     *current_file;
	qdb_target_t debug_target;
}
-initWithTarget:(qdb_target_t) target;
-handleDebugEvent;
@end

#endif//__qwaq_debugger_h
