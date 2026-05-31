#include <Array.h>
#include <string.h>
#include <types.h>

#include "ruamoko/qwaq/ui/table.h"
#include "ruamoko/qwaq/debugger/debugger.h"
#include "ruamoko/qwaq/debugger/typeencodings.h"

void printf(string fmt, ...);

@implementation Debugger
-initWithTarget:(qdb_target_t) target
	fileManager:(id<DebugGetFile>) fileManager
{
	if (!(self = [super init])) {
		return nil;
	}
	self.target = target;
	self.file_manager = [fileManager retain];

	return self;
}

+(Debugger *)withTarget:(qdb_target_t)target
			fileManager:(id<DebugGetFile>) fileManager
{
	return [[[self alloc] initWithTarget:target
							 fileManager:fileManager] autorelease];
}

-(qdb_target_t)target
{
	return target;
}

-(void)dealloc
{
	[locals_data release];
	[file_manager release];
	[super dealloc];
}

-(void) setup
{
	qdb_state_t state = qdb_get_state (target);

	locals_data = [[LocalsData withTarget:target] retain];
	locals_view = [file_manager showData:"Locals" for:self];
	[locals_view addColumn:[TableColumn named:"name" width:12]];
	[locals_view addColumn:[[TableColumn named:"value" width:26]
							setGrowMode:true]];
	[locals_view setDataSource:locals_data];
}

-(void) show_line
{
	qdb_state_t state = qdb_get_state (target);
	string      filename = state.file;
	if (filename) {
		string      filepath = qdb_get_file_path (target, filename);
		id<DebugFile> file = [file_manager showFile:filename path:filepath];
		[file setDebugger:self];
		if (state.line) {
			[[file gotoLine:state.line - 1] highlightLine];
		}
	} else {
	}
}

-(void)update_watchvars
{
	qdb_state_t state = qdb_get_state (target);
	[locals_data setFunction:state.func];
	[locals_data fetchData];
}

-run:(id<DebugFile>)file
{
	return self;
}

-gotoCursor:(id<DebugFile>)file
{
	string      filename = [file filename];
	unsigned    line = [file cursor].y + 1;
	unsigned    addr = qdb_get_source_line_addr (self.target,
												 filename, line);
	int         set = -1;
	if (addr) {
		set = qdb_set_breakpoint (self.target, addr);
	}
	if (set >= 0) {
		qdb_set_trace (self.target, 0);
		if (set) {
			self.breakHandler = @selector(breakKeep);
		} else {
			self.breakHandler = @selector(breakClear);
		}
		self.running = 1;
		qdb_continue (self.target);
	} else {
	}
	return self;
}

-traceInto:(id<DebugFile>)file
{
	self.traceHandler = @selector(traceStep);
	qdb_set_trace (self.target, 1);
	self.trace_cond.state = qdb_get_state (self.target);
	self.running = 1;
	qdb_continue (self.target);
	return self;
}

-stepOver:(id<DebugFile>)file
{
	self.traceHandler = @selector(traceNext);
	qdb_set_trace (self.target, 1);
	self.trace_cond.state = qdb_get_state (self.target);
	self.trace_cond.depth = qdb_get_stack_depth (self.target);
	self.running = 1;
	qdb_continue (self.target);
	return self;
}

-stop:(prdebug_t)reason
{
	running = 0;
	if (!locals_data) {
		[self setup];
	}
	[self show_line];
	[self update_watchvars];
	return self;
}

// stop only if the progs have not advanced (may be a broken jump)
// or the progs have advanced to a different source line
static int
is_new_line (qdb_state_t last_state, qdb_state_t state)
{
	return !(last_state.staddr != state.staddr
			 && last_state.func == state.func
			 && last_state.file == state.file
			 && last_state.line == state.line);
}

-traceStep
{
	qdb_state_t state = qdb_get_state (target);

	if (trace_cond.until_function && trace_cond.until_function == state.func) {
		trace_cond.until_function = 0;
		[self stop:prd_trace];
		return self;
	}
	if (is_new_line(trace_cond.state, state)) {
		[self stop:prd_trace];
		return self;
	}
	trace_cond.state = state;
	qdb_continue (self.target);
	return self;
}

-traceNext
{
	qdb_state_t state = qdb_get_state (target);
	if (trace_cond.until_function && trace_cond.until_function == state.func) {
		trace_cond.until_function = 0;
		[self stop:prd_trace];
		return self;
	}
	if (is_new_line(trace_cond.state, state)
		&& qdb_get_stack_depth (target) <= trace_cond.depth) {
		[self stop:prd_trace];
		return self;
	}
	trace_cond.state = state;
	qdb_continue (self.target);
	return self;
}

-breakKeep
{
	return self;
}

-breakClear
{
	qdb_state_t state = qdb_get_state (target);
	qdb_clear_breakpoint (target, state.staddr);
	return self;
}

-handleDebugEvent
{
	if (qdb_get_event (target, &event)) {
		switch (event.what) {
			case prd_none:
				break;	// shouldn't happen
			case prd_trace:
				[self performSelector:traceHandler];
				break;
			case prd_break_point:
			case prd_watch_point:
				[self performSelector:breakHandler];
				[self stop:event.what];
				break;
			case prd_sub_enter:
				if (sub_cond.onEnter) {
					[self stop:event.what];
				} else {
					qdb_continue (self.target);
				}
				break;
			case prd_sub_exit:
				if (sub_cond.onExit) {
					[self stop:event.what];
				} else {
					qdb_continue (self.target);
				}
				break;
			case prd_func_enter:
			case prd_func_exit:
				break;
			case prd_begin:
				trace_cond.until_function = event.function;
				[self stop:event.what];
				break;
			case prd_terminate:
				printf("Program ended: %d\n", event.exit_code);
				[self stop:event.what];
				break;
			case prd_run_error:
			case prd_error:
				printf("%s\n", event.message);
				[self stop:event.what];
				break;
		}
	}
	return self;
}

@end
