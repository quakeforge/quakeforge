int fence;

#include <AutoreleasePool.h>
#include <key.h>

#include "color.h"
#include "qwaq-app.h"
#include "qwaq-button.h"
#include "qwaq-curses.h"
#include "qwaq-editor.h"
#include "qwaq-group.h"
#include "qwaq-listener.h"
#include "qwaq-window.h"
#include "qwaq-screen.h"
#include "qwaq-view.h"

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

	initialize ();
	init_pair (1, COLOR_WHITE, COLOR_BLUE);
	init_pair (2, COLOR_WHITE, COLOR_BLACK);
	init_pair (3, COLOR_BLACK, COLOR_GREEN);
	init_pair (4, COLOR_YELLOW, COLOR_RED);

	screen = [TextContext screen];
	screenSize = [screen size];
	objects = [[Group alloc] initWithContext: screen owner: nil];

	[screen bkgd: COLOR_PAIR (1)];
	[screen scrollok: 1];
	Rect r = {nil, screenSize};
	r.offset.x = r.extent.width / 4;
	r.offset.y = r.extent.height / 4;
	r.extent.width /= 2;
	r.extent.height /= 2;
	Window *w;
	[objects insertSelected: w = [[Window windowWithRect: r] setBackground: COLOR_PAIR (2)]];
	r = {{1, 1}, {r.extent.width - 2, r.extent.height - 2}};
	[w insertSelected: [[Editor alloc] initWithRect: r file: "Makefile"]];
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

-draw
{
	[objects draw];
	[TextContext refresh];
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	if (event.what == qe_resize) {
		Extent delta;
		delta.width = event.resize.width - screenSize.width;
		delta.height = event.resize.height - screenSize.height;

		resizeterm (event.resize.width, event.resize.height);
		[screen resizeTo: {event.resize.width, event.resize.height}];
		screenSize = [screen size];
		[objects resize: delta];
		[screen refresh];
		event.what = qe_none;
		return self;
	}
	if (event.what == qe_key
		&& (event.key.code == '\x18' || event.key.code == '\x11')) {
		event.what = qe_command;
		event.message.int_val = qc_exit;
	}
	if (event.what == qe_command
		&& (event.message.int_val == qc_exit
			|| event.message.int_val == qc_error)) {
		endState = event.message.int_val;;
	}
	[objects handleEvent: event];
	return self;
}
@end

int main (int argc, string *argv)
{
	fence = 0;
	//while (!fence) {}

	id app = [[QwaqApplication app] retain];

	[app run];
	[app release];
	qwaq_event_t event;
	get_event (&event);	// XXX need a "wait for queue idle"
	return 0;
}
