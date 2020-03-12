#include "qwaq-draw.h"
#include "qwaq-textcontext.h"

@implementation TextContext
+ (int) is_initialized = #0;
+ (void) initialize
{
	if (![self is_initialized]) {
		initialize ();
	}
}

+ (int) max_colors = #0;
+ (int) max_color_pairs = #0;
+ (void) init_pair: (int) pair, int fg, int bg = #0;
+ (int) acs_char: (int) acs = #0;
+ (void) move: (Point) pos = #0;
+ (void) curs_set: (int) visibility = #0;
+ (void) doupdate = #0;

static TextContext *screen;
+ (TextContext *) screen
{
	if (!screen) {
		screen = [[TextContext alloc] init];
	}
	return screen;
}

- init
{
	if (!(self = [super init])) {
		return nil;
	}
	window = stdscr;
	rect = getwrect (window);
	return self;
}

- initWithRect: (Rect) rect
{
	if (!(self = [super init])) {
		return nil;
	}
	window = create_window (rect.offset.x, rect.offset.y,
							rect.extent.width, rect.extent.height);
	offset = {};
	size = rect.extent;
	return self;
}

- initWithWindow: (window_t) window
{
	if (!(self = [super init])) {
		return nil;
	}
	self.window = window;
	return self;
}

-(window_t) window
{
	return window;
}

-(Extent) size
{
	return size;
}

- blitFromBuffer: (DrawBuffer *) srcBuffer to: (Point) pos from: (Rect) rect
{
	Extent srcSize = [srcBuffer size];
	Rect r = { {}, size };
	Rect t = { pos, rect.extent };

	wprintf (stdscr, "src: %p\n", srcBuffer);
	wprintf (stdscr, "srcSize: %d %d\n", srcSize.width, srcSize.height);
	wprintf (stdscr, "pos: %d %d\n", pos.x, pos.x);
	wprintf (stdscr, "rect: %d %d %d %d\n",
			 rect.offset.x, rect.offset.y,
			 rect.extent.width, rect.extent.height);
	wprintf (stdscr, "r: %d %d %d %d\n",
			 r.offset.x, r.offset.y, r.extent.width, r.extent.height);
	wprintf (stdscr, "t: %d %d %d %d\n",
			 t.offset.x, t.offset.y, t.extent.width, t.extent.height);
	t = clipRect (r, t);
	wprintf (stdscr, "t: %d %d %d %d\n",
			 t.offset.x, t.offset.y, t.extent.width, t.extent.height);
	if (t.extent.width < 0 || t.extent.height < 0) {
		return self;
	}

	rect.offset.x += t.offset.x - pos.x;
	rect.offset.y += t.offset.y - pos.y;
	rect.extent = t.extent;
	pos = t.offset;

	r.offset = nil;
	r.extent = size;

	rect = clipRect (r, rect);
	wprintf (stdscr, "pos: %d %d\n", pos.x, pos.x);
	wprintf (stdscr, "rect: %d %d %d %d\n",
			 rect.offset.x, rect.offset.y,
			 rect.extent.width, rect.extent.height);
	wprintf (stdscr, "r: %d %d %d %d\n",
			 r.offset.x, r.offset.y, r.extent.width, r.extent.height);
	if (rect.extent.width < 0 || rect.extent.height < 0) {
		return self;
	}

	int        *src = [srcBuffer buffer]
					  + rect.offset.y * srcSize.width + rect.offset.x;
	for (int y = 0; y < rect.extent.height; y++) {
		mvwblit_line (window, pos.x, y + pos.y, src, rect.extent.width);
		src += srcSize.width;
	}
	return self;
}

- (void) mvprintf: (Point) pos, string fmt, ... = #0;
- (void) printf: (string) fmt, ... = #0;
- (void) vprintf: (string) mft, @va_list args = #0;
- (void) addch: (int) ch = #0;
- (void) mvvprintf: (Point) pos, string mft, @va_list args = #0;
- (void) refresh = #0;
+ (void) refresh = #0;
- (void) mvaddch: (Point) pos, int ch = #0;
- (void) bkgd: (int) ch = #0;
- (void) scrollok: (int) flag = #0;
- (void) border: (box_sides_t) sides, box_corners_t corners = #0;

@end

window_t stdscr = (window_t) 1;

void initialize (void) = #0;
window_t create_window (int xpos, int ypos, int xlen, int ylen) = #0;
void destroy_window (window_t win) = #0;
void mvwprintf (window_t win, int x, int y, string fmt, ...) = #0;
void wprintf (window_t win, string fmt, ...) = #0;
void wvprintf (window_t win, string fmt, @va_list args) = #0;
void mvwvprintf (window_t win, int x, int y, string fmt, @va_list args) = #0;
void wrefresh (window_t win) = #0;
void mvwaddch (window_t win, int x, int y, int ch) = #0;
void waddch (window_t win, int ch) = #0;
int get_event (qwaq_event_t *event) = #0;
int max_colors (void) = #0;
int max_color_pairs (void) = #0;
int init_pair (int pair, int f, int b) = #0;
void wbkgd (window_t win, int ch) = #0;
void scrollok (window_t win, int flag) = #0;
int acs_char (int acs) = #0;

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
int curs_set (int visibility) = #0;
int move (int x, int y) = #0;
void wborder (window_t window, box_sides_t sides, box_corners_t corners) = #0;
void mvwblit_line (window_t window, int x, int y, int *wch, int len) = #0;
Rect getwrect (window_t window) = #0;

void printf(string fmt, ...) = #0;
