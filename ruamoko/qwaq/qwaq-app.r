int fence;
#include <AutoreleasePool.h>

#include "color.h"
#include "qwaq-app.h"
#include "qwaq-button.h"
#include "qwaq-curses.h"
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
	init_pair (3, COLOR_WHITE, COLOR_GREEN);
	init_pair (4, COLOR_YELLOW, COLOR_RED);

	screen = [TextContext screen];
	objects = [[Group alloc] initWithContext: screen owner: nil];

	[screen bkgd: COLOR_PAIR (1)];
	Rect r = { nil, [screen size] };
	r.offset.x = r.extent.width / 4;
	r.offset.y = r.extent.height / 4;
	r.extent.width /= 2;
	r.extent.height /= 2;
	Window *w;
	[objects insert: w = [[Window windowWithRect: r] setBackground: COLOR_PAIR (2)]];
	DrawBuffer *released = [DrawBuffer buffer: {12, 1}];
	DrawBuffer *pressed = [DrawBuffer buffer: {12, 1}];
	Button *b = [[Button alloc] initWithPos: {3, 4} releasedIcon: released
													pressedIcon: pressed];
	[w addView: b];
	[released bkgd: COLOR_PAIR(3)];
	[released clear];
	[released mvaddstr: {2, 0}, "press me"];
	[pressed bkgd: COLOR_PAIR(4)];
	[pressed clear];
	[pressed mvaddstr: {1, 0}, "release me"];
	[[b onPress] addListener: self message: @selector (buttonPressed:)];
	[[b onRelease] addListener: self message: @selector (buttonReleased:)];
	[[b onClick] addListener: self message: @selector (buttonClick:)];
	[[b onDrag] addListener: self message: @selector (buttonDrag:)];
	[[b onAuto] addListener: self message: @selector (buttonAuto:)];
	return self;
}

-(void) buttonPressed: (id) sender
{
	[screen mvaddstr: {2, 0}, " pressed"];
	[screen refresh];
}

-(void) buttonReleased: (id) sender
{
	[screen mvaddstr: {2, 0}, "released"];
	[screen refresh];
}

-(void) buttonClick: (id) sender
{
	[screen mvaddstr: {2, 0}, "clicked "];
	[screen refresh];
}

-(void) buttonDrag: (id) sender
{
	[screen mvaddstr: {2, 0}, "dragged "];
	Rect rect = [sender rect];
	[screen mvprintf: {15, 0}, "%d %d", rect.offset.x, rect.offset.y];
	[screen refresh];
}

-(void) buttonAuto: (id) sender
{
	[screen mvprintf: {2, 1}, "%d", autocount++];
	[screen refresh];
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

-draw
{
	[objects draw];
	[TextContext refresh];
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	[objects handleEvent: event];
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
