#include <QF/keys.h>
#include <Array.h>
#include <string.h>
#include <types.h>

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
	[[current_file onEvent] addListener: self :@selector(key_event::)];
	//FIXME id<View>?
	[source_window insertSelected: (View *) file_proxy];
	[source_window setTitle: [current_file filename]];
	[source_window redraw];

	locals_window = [[Window alloc] initWithRect: {{0, 0}, {40, 10}}];
	[locals_window setBackground: color_palette[064]];
	[locals_window setTitle: "Locals"];
	locals_view = [[View alloc] initWithRect: {{1, 1}, {38, 8}}];
	[locals_window insertSelected: locals_view];
	[application addView: locals_window];
}

-(void) show_line
{
	qdb_state_t state = qdb_get_state (debug_target);
	Editor     *file = [self find_file: state.file];

	printf ("%s:%d\n", state.file, state.line);
	if (current_file != file) {
		[[current_file onEvent] removeListener:self :@selector(key_event::)];
		[file_proxy setView:file];
		[[file onEvent] addListener:self :@selector(key_event::)];
		[source_window setTitle: [file filename]];
		current_file = file;
	}
	[[current_file gotoLine:state.line - 1] highlightLine];
	[source_window redraw];
}

static void
update_current_func (Debugger *self, unsigned fnum)
{
	self.current_fnum = fnum;
	if (self.aux_func) {
		obj_free (self.aux_func);
		self.aux_func = nil;
	}
	if (self.local_defs) {
		obj_free (self.local_defs);
	}
	if (self.local_data) {
		obj_free (self.local_data);
	}
	self.func = qdb_get_function (self.debug_target, fnum);
	self.aux_func = qdb_get_auxfunction (self.debug_target, fnum);
	if (self.aux_func) {
		self.local_defs = qdb_get_local_defs (self.debug_target, fnum);
	}
	if (self.func) {
		self.local_data = obj_malloc (self.func.local_size);
	}
}

-(void)update_watchvars
{
	qdb_state_t state = qdb_get_state (debug_target);
	if (state.func != current_fnum) {
		update_current_func (self, state.func);
	}
	if (!local_data) {
		return;
	}
	qdb_get_data (debug_target, func.local_data, func.local_size, local_data);
	if (!local_defs) {
		[locals_view mvprintf:{0,0}, "%d", func.local_size];
		for (int y = 1; y < [locals_view size].height; y++) {
			unsigned    ind = (y - 1) * 4;
			if (ind < func.local_size) {
				[locals_view mvprintf:{0, y}, "%02x", ind];
				for (int x = 0; x < 4 && ind < func.local_size; x++, ind++) {
					[locals_view mvprintf:{x * 9 + 3, y}, "%08x",
										  local_data[ind]];
				}
			} else {
				break;
			}
		}
	} else {
		for (int y = 0; y < [locals_view size].height; y++) {
			if (y >= aux_func.num_locals) {
				break;
			}
			qdb_def_t *def = local_defs + y;
			[locals_view mvprintf:{0, y}, "%s",
								  qdb_get_string (debug_target, def.name)];
			string      valstr = "--";
			printf ("def type_size %d\n", def.type_size);
			switch (def.type_size & 0xffff) {
				case ev_void:
				case ev_invalid:
				case ev_type_count:
					break;
				case ev_string:
				case ev_float:
				case ev_vector:
				case ev_entity:
				case ev_field:
				case ev_func:
				case ev_pointer:
				case ev_quat:
				case ev_integer:
				case ev_uinteger:
				case ev_short:
				case ev_double:
					valstr = sprintf ("%d", def.type_size >> 16);
					break;
			}
			int         x = [locals_view size].width - strlen (valstr) - 1;
			[locals_view mvaddstr:{x, y}, valstr];
		}
	}
	[TextContext refresh];
}

static int
key_event (Debugger *self, Editor *file, qwaq_event_t *event)
{
	if (event.what == qe_mouseclick && !(event.mouse.buttons & 0x78)) {
		printf ("%s\n", [file getWordAt: {event.mouse.x, event.mouse.y}]);
		[self.source_window redraw];
	} else if (event.what == qe_keydown) {
		switch (event.key.code) {
			case QFK_F7:
				qdb_set_trace (self.debug_target, 1);
				qdb_continue (self.debug_target);
				return 1;
		}
	}
	return 0;
}

-(void)key_event:(Editor *)editor :(qwaq_event_t *)event
{
	if (key_event (self, editor, event)) {
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

void traceon() = #0;
void traceoff() = #0;

void qdb_set_trace (qdb_target_t target, int state) = #0;
int qdb_set_breakpoint (qdb_target_t target, unsigned staddr) = #0;
int qdb_clear_breakpoint (qdb_target_t target, unsigned staddr) = #0;
int qdb_set_watchpoint (qdb_target_t target, unsigned offset) = #0;
int qdb_clear_watchpoint (qdb_target_t target) = #0;
int qdb_continue (qdb_target_t target) = #0;
qdb_state_t qdb_get_state (qdb_target_t target) = #0;
int qdb_get_data (qdb_target_t target, unsigned src, unsigned len,
				  void *dst) = #0;
string qdb_get_string (qdb_target_t target, unsigned str) = #0;
qdb_def_t qdb_find_global (qdb_target_t target, string name) = #0;
qdb_def_t qdb_find_field (qdb_target_t target, string name) = #0;
qdb_function_t *qdb_find_function (qdb_target_t target, string name) = #0;
qdb_function_t *qdb_get_function (qdb_target_t target, unsigned fnum) = #0;
qdb_auxfunction_t *qdb_find_auxfunction (qdb_target_t target,
										 string name) = #0;
qdb_auxfunction_t *qdb_get_auxfunction (qdb_target_t target,
										unsigned fnum) = #0;
qdb_def_t *qdb_get_local_defs (qdb_target_t target, unsigned fnum) = #0;
