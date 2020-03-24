#include <Array.h>

#include "qwaq-curses.h"
#include "qwaq-debugger.h"
#include "qwaq-editor.h"
#include "qwaq-proxyview.h"
#include "qwaq-window.h"

@implementation Debugger

-initWithTarget:(qdb_target_t) target
{
	if (!(self = [super init])) {
		return nil;
	}
	debug_target = target;

	files = [[Array array] retain];
	//FIXME need a window manager
	source_window = [[Window alloc] initWithRect: getwrect (stdscr)];

	return self;
}

-(Editor *) find_file:(string) filename
{
	Editor     *file;
	for (int i = [files count]; i-- > 0; ) {
		file = [files objectAtIndex: i];
		if ([file filename] == filename) {
			return file;
		}
	}
	Rect rect = {{1, 1}, [source_window size]};
	rect.extent.width -= 2;
	rect.extent.height -= 2;
	file = [[Editor alloc] initWithRect: rect file: filename];
	[files addObject: file];
	return file;
}

-(void) setup
{
	qdb_state_t state = qdb_get_state (debug_target);

	current_file = [self find_file: state.file];
	file_proxy = [[ProxyView alloc] initWithView: current_file];
	//FIXME id<View>?
	[source_window insertSelected: (View *) file_proxy];
}

-handleDebugEvent
{
	if (!file_proxy) {
		[self setup];
	}
	return self;
}

@end

void qdb_set_trace (qdb_target_t target, int state) = #0;
int qdb_set_breakpoint (qdb_target_t target, unsigned staddr) = #0;
int qdb_clear_breakpoint (qdb_target_t target, unsigned staddr) = #0;
int qdb_set_watchpoint (qdb_target_t target, unsigned offset) = #0;
int qdb_clear_watchpoint (qdb_target_t target) = #0;
int qdb_continue (qdb_target_t target) = #0;
qdb_state_t qdb_get_state (qdb_target_t target) = #0;
