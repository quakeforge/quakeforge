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
	[screen setBackground: COLOR_PAIR (1)];
	Rect r = screen.rect;
	wprintf (screen.window, "%d %d %d %d\n", r.xpos, r.ypos, r.xlen, r.ylen);
	r.xpos = r.xlen / 4;
	r.ypos = r.ylen / 4;
	r.xlen /= 2;
	r.ylen /= 2;
	wprintf (screen.window, "%d %d %d %d\n", r.xpos, r.ypos, r.xlen, r.ylen);
	wrefresh(screen.window);
	Window *w;
	[screen add: w=[[Window windowWithRect: r] setBackground: COLOR_PAIR (2)]];
	wprintf (w.window, "%d %d %d %d\n", r.xpos, r.ypos, r.xlen, r.ylen);
	return self;
}

-run
{
	do {
		arp_start ();

		update_panels ();
		doupdate ();
		get_event (&event);
		if (event.event_type != qe_none) {
			[self handleEvent: &event];
		}

		arp_end ();
	} while (!endState);
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	[screen handleEvent: event];
	if (event.event_type == qe_key && event.e.key == '\x18') {
		event.event_type = qe_command;
		event.e.message.command = qc_exit;
	}
	if (event.event_type == qe_command
		&& (event.e.message.command == qc_exit
			|| event.e.message.command == qc_error)) {
		endState = event.e.message.command;
	}
	return self;
}
@end

int main (int argc, string *argv)
{
	id app = [[QwaqApplication app] retain];

	[app run];
	[app release];
	qwaq_event_t event;
	get_event (&event);	// XXX need a "wait for queue idle"
	return 0;
}

window_t stdscr = (window_t) 1;

void initialize (void) = #0;
window_t create_window (int xpos, int ypos, int xlen, int ylen) = #0;
void destroy_window (window_t win) = #0;
void mvwprintf (window_t win, int x, int y, string fmt, ...) = #0;
void wprintf (window_t win, string fmt, ...) = #0;
void wrefresh (window_t win) = #0;
int get_event (qwaq_event_t *event) = #0;
int max_colors (void) = #0;
int max_color_pairs (void) = #0;
int init_pair (int pair, int f, int b) = #0;
void wbkgd (window_t win, int ch) = #0;

panel_t create_panel (window_t window) = #0;
void destroy_panel (panel_t panel) = #0;
void hide_panel (panel_t panel) = #0;
void show_panel (panel_t panel) = #0;
void top_panel (panel_t panel) = #0;
void bottom_panel (panel_t panel) = #0;
void move_panel (panel_t panel, int x, int y) = #0;
window_t panel_window (panel_t panel) = #0;
void update_panels (void) = #0;
void doupdate (void) = #0;
