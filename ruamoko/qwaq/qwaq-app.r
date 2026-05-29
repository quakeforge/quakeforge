int fence;

#include <QF/keys.h>

#include <AutoreleasePool.h>

#include "ruamoko/qwaq/editor/editor.h"
#include "ruamoko/qwaq/ui/color.h"
#include "ruamoko/qwaq/ui/curses.h"
#include "ruamoko/qwaq/ui/group.h"
#include "ruamoko/qwaq/ui/scrollbar.h"
#include "ruamoko/qwaq/ui/view.h"
#include "ruamoko/qwaq/ui/window.h"
#include "ruamoko/qwaq/debugger/debugger.h"
#include "ruamoko/qwaq/qwaq-app.h"
#include "ruamoko/qwaq/qwaq-input.h"

void init_input (void) = #0;//FIXME

int color_palette[64];

static AutoreleasePool *autorelease_pool;
static void
arp_start (void)
{
	autorelease_pool = [[AutoreleasePool alloc] init];
}

static void
arp_end (void)
{
	[autorelease_pool release];
	autorelease_pool = nil;
}

@interface EditWindow : Window <DebugFile>
{
	Editor *editor;
	Debugger *debugger;
}
+(EditWindow *)withRect:(Rect) rect Editor:(Editor*) editor;
@end
@implementation EditWindow
-(EditWindow *)initWithRect:(Rect) rect Editor:(Editor*) editor
{
	if (!(self = [super initWithRect:rect])) {
		return nil;
	}
	self.editor = editor;

	auto s = rect.extent;
	auto vertical = [ScrollBar vertical:s.height - 2 at:{s.width - 1, 1}];
	[self insert:vertical];
	[self insert:[EditStatus withRect:{{1, 0}, {2, 1}}]];

	[self insertSelected:editor];
	[editor setVerticalScrollBar:vertical];
	[self setTitle:[editor filename]];

	return self;
}

+(EditWindow *)withRect:(Rect) rect Editor:(Editor*) editor
{
	return [[[self alloc] initWithRect:rect Editor:editor] autorelease];
}

-handleEvent: (qwaq_event_t *) event
{
	printf ("handleEvent: %d %@\r\n", event.what, debugger);
	if (event.what == qe_key) {
		switch (event.key.code) {
			case QFK_F7:
				[debugger traceInto:self];
				event.what = qe_none;
				break;
			case QFK_F8:
				[debugger stepOver:self];
				event.what = qe_none;
				break;
			case QFK_F4:
				[debugger gotoCursor:self];
				event.what = qe_none;
				break;
		}
	}
	if (event.what != qe_none) {
		[super handleEvent: event];
	}
	return self;
}

-gotoLine:(int)line
{
	[editor gotoLine:line];
	[editor redraw];
	return self;
}

-highlightLine
{
	[editor highlightLine];
	[editor redraw];
	return self;
}

-(string)filename
{
	return [editor filename];
}

-(uvec2)cursor
{
	return @bitcast(uvec2, [editor cursor]);
}

-setDebugger:(Debugger *)debugger
{
	self.debugger = debugger;
	return self;
}
@end

@implementation QwaqApplication
+app
{
	return [[[self alloc] init] autorelease];
}

-init
{
	if (!(self = [super init])) {
		return nil;
	}

	init_input ();
	initialize ();
	for (int i = 1; i < 64; i++) {
		init_pair (i, i & 0x7, i >> 3);
		color_palette[i] = COLOR_PAIR (i);
	}

	screen = [TextContext screen];
	screenSize = [screen size];
	objects = [[Group withContext: screen owner: nil] retain];

	[screen bkgd: color_palette[047]];
	[screen scrollok: 1];
	[screen clear];
	wrefresh (stdscr);//FIXME

	debuggers = [[Array array] retain];
	editors = [[Array array] retain];
	return self;
}

-(Extent)size
{
	return screenSize;
}

-(TextContext *)screen
{
	return screen;
}

-draw
{
	[objects draw];
	[screen refresh];
	return self;
}

-(Debugger *)find_debugger:(qdb_target_t) target
{
	Debugger   *debugger;

	for (int i = [debuggers count]; i-- > 0; ) {
		debugger = [debuggers objectAtIndex: i];
		if ([debugger target] == target) {
			return debugger;
		}
	}
	debugger = [Debugger withTarget: target fileManager:self];
	[debuggers addObject: debugger];
	return debugger;
}

-handleEvent: (qwaq_event_t *) event
{
	switch (event.what) {
		case qe_axis:
		case qe_button:
			event.what = qe_none;
			break;
		case qe_resize:
			Extent delta;
			delta.width = event.resize.width - screenSize.width;
			delta.height = event.resize.height - screenSize.height;

			resizeterm (event.resize.width, event.resize.height);
			[screen resizeTo: {event.resize.width, event.resize.height}];
			screenSize = [screen size];
			[objects resize: delta];
			[screen refresh];
			event.what = qe_none;
			break;
		case qe_key:
			if (event.key.code == '\x18' || event.key.code == '\x11') {
				endState = event.message.int_val;
				event.what = qe_none;
			}
			break;
		case qe_debug_event:
			[[self find_debugger:(qdb_target_t)event.message.int_val]
				handleDebugEvent];
			event.what = qe_none;
			break;
	}
	if (event.what != qe_none) {
		[objects handleEvent: event];
	}
	return self;
}

-run
{
	[objects takeFocus];
	[self draw];
	do {
		arp_start ();

		get_event (&event);
		if (event.what != qe_none) {
			[self handleEvent: &event];
		}

		arp_end ();
	} while (!endState);
	return self;
}

-addView:(View *)view
{
	[objects insertSelected: view];
	[screen refresh];
	return self;
}

-(id<DebugFile>)showFile:(string)filename path:(string)filepath
{
	for (int i = [editors count]; i-- > 0; ) {
		EditWindow *ew = [editors objectAtIndex:i];
		if ([ew filename] == filename) {
			[ew raise];
			return ew;
		}
	}
	Extent s = [application size];
	Rect rect = {{1, 1}, {s.width - 2, s.height - 2}};
	Editor *editor = [Editor withRect:rect file:filename path:filepath];
	EditWindow *ew = [EditWindow withRect:{nil, s} Editor:editor];
	[editors addObject:ew];
	[self addView:ew];
	return ew;
}

@end

QwaqApplication *application;

int main (int argc, string *argv)
{
	fence = 0;
	//while (!fence) {}

	arp_start ();
	application = [[QwaqApplication app] retain];
	arp_end ();

	[application run];
	[application release];
	qwaq_event_t event;
	get_event (&event);	// XXX need a "wait for queue idle"

	return 0;
}
