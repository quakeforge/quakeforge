int fence;
#include <AutoreleasePool.h>

#include "color.h"
#include "qwaq-app.h"
#include "qwaq-curses.h"
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

	screen = [[Screen screen] retain];
	[self insert:screen];
	[screen setBackground: COLOR_PAIR (1)];
	Rect r = *[screen getRect];
	r.offset.x = r.extent.width / 4;
	r.offset.y = r.extent.height / 4;
	r.extent.width /= 2;
	r.extent.height /= 2;
	Window *w;
	[self insert: w=[[Window windowWithRect: r] setBackground: COLOR_PAIR (2)]];
	//wprintf (w.window, "%d %d %d %d\n", r.offset.x, r.offset.y, r.extent.width, r.ylen);
	return self;
}

-run
{
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

-handleEvent: (qwaq_event_t *) event
{
	[screen handleEvent: event];
	if (event.what == qe_key && event.key == '\x18') {
		event.what = qe_command;
		event.message.command = qc_exit;
	}
	if (event.what == qe_command
		&& (event.message.command == qc_exit
			|| event.message.command == qc_error)) {
		endState = event.message.command;
	}
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
