#include <QF/keys.h>
#include <Array.h>
#include <string.h>
#include <types.h>

#include "ui/curses.h"
#include "ui/listener.h"
#include "ui/proxyview.h"
#include "ui/window.h"
#include "debugger/debugger.h"
#include "debugger/typeencodings.h"
#include "editor/editor.h"
#include "qwaq-app.h"

@implementation Debugger
-(qdb_target_t)target
{
	return target;
}

-initWithTarget:(qdb_target_t) target
{
	if (!(self = [super init])) {
		return nil;
	}
	self.target = target;

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
	qdb_state_t state = qdb_get_state (target);

	current_file = [self find_file: state.file];
	file_proxy = [[ProxyView alloc] initWithView: current_file];
	[[current_file gotoLine:state.line - 1] highlightLine];
	[[current_file onEvent] addListener: self :@selector(proxy_event::)];
	//FIXME id<View>?
	[source_window insertSelected: (View *) file_proxy];
	[source_window setTitle: [current_file filename]];
	[source_window redraw];

	locals_window = [[Window alloc] initWithRect: {{0, 0}, {40, 10}}];
	[locals_window setBackground: color_palette[064]];
	[locals_window setTitle: "Locals"];
	locals_view = [[LocalsView alloc] initWithRect: {{1, 1}, {38, 8}}
											target: target];
	[locals_window insertSelected: locals_view];
	[application addView: locals_window];

	[[locals_view onEvent] addListener:self :@selector(proxy_event::)];
}

-(void) show_line
{
	qdb_state_t state = qdb_get_state (target);
	Editor     *file = [self find_file: state.file];

	printf ("%s:%d\n", state.file, state.line);
	if (current_file != file) {
		[[current_file onEvent] removeListener:self :@selector(proxy_event::)];
		[file_proxy setView:file];
		[[file onEvent] addListener:self :@selector(proxy_event::)];
		[source_window setTitle: [file filename]];
		current_file = file;
	}
	[[current_file gotoLine:state.line - 1] highlightLine];
	[source_window redraw];
}

-(void)update_watchvars
{
	qdb_state_t state = qdb_get_state (target);
	[locals_view setFunction:state.func];
	[locals_view redraw];
}

static int
proxy_event (Debugger *self, id proxy, qwaq_event_t *event)
{
	if (event.what == qe_mouseclick && !(event.mouse.buttons & 0x78)) {
		if (proxy == self.current_file) {
			printf ("%s\n", [proxy getWordAt: {event.mouse.x, event.mouse.y}]);
			[self.source_window redraw];
			return 1;
		}
	} else if (event.what == qe_keydown) {
		switch (event.key.code) {
			case QFK_F7:
				qdb_set_trace (self.target, 1);
				qdb_continue (self.target);
				return 1;
		}
	}
	return 0;
}

-(void)proxy_event:(id)proxy :(qwaq_event_t *)event
{
	if (proxy_event (self, proxy, event)) {
		event.what = qe_none;
	}
}

-handleDebugEvent
{
	if (!file_proxy) {
		[self setup];
	}
	[self show_line];
	[self update_watchvars];
	return self;
}

@end
