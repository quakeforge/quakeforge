#include <Array.h>
#include <QF/keys.h>

#include "qwaq-app.h"
#include "qwaq-curses.h"
#include "qwaq-debugger.h"
#include "qwaq-editor.h"
#include "qwaq-listener.h"
#include "qwaq-proxyview.h"
#include "qwaq-window.h"

@implementation Debugger
-(qdb_target_t)debug_target
{
	return debug_target;
}

-initWithTarget:(qdb_target_t) target
{
	if (!(self = [super init])) {
		return nil;
	}
	debug_target = target;

	files = [[Array array] retain];
	source_window = [[Window alloc] initWithRect: {nil, [application size]}];
	[application addView:source_window];

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
	[[current_file gotoLine:state.line - 1] highlightLine];
	[[current_file onEvent] addListener: self :@selector(key_event:)];
	//FIXME id<View>?
	[source_window insertSelected: (View *) file_proxy];
	[source_window setTitle: [current_file filename]];
	[source_window redraw];
}

-(void) show_line
{
	qdb_state_t state = qdb_get_state (debug_target);
	Editor     *file = [self find_file: state.file];

	printf ("%s:%d\n", state.file, state.line);
	if (current_file != file) {
		[[current_file onEvent] removeListener:self :@selector(key_event:)];
		[file_proxy setView:file];
		[[file onEvent] addListener:self :@selector(key_event:)];
		[source_window setTitle: [file filename]];
		current_file = file;
	}
	[[current_file gotoLine:state.line - 1] highlightLine];
	[source_window redraw];
}

-(void)key_event: (ed_event_t *)_event
{
	qwaq_event_t *event = _event.event;
	if (event.what == qe_keydown) {
		switch (event.key.code) {
			case QFK_F7:
				qdb_set_trace (debug_target, 1);
				qdb_continue (debug_target);
				break;
			default:
				return;
		}
	}
	event.what = qe_none;
}

-handleDebugEvent
{
	if (!file_proxy) {
		[self setup];
	}
	[self show_line];
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
