#include "color.h"
#include "event.h"

typedef struct window_s *window_t;

window_t stdscr = (window_t) 1;

void initialize (void) = #0;
window_t create_window (int xpos, int ypos, int xlen, int ylen) = #0;
void destroy_window (window_t win) = #0;
void mvwprintf (window_t win, int x, int y, string fmt, ...) = #0;
void wrefresh (window_t win) = #0;

int get_event (qwaq_event_t *event) = #0;
int max_colors (void) = #0;
int max_color_pairs (void) = #0;
int init_pair (int pair, int f, int b) = #0;
void wbkgd (window_t win, int ch) = #0;

int main (int argc, string *argv)
{
	int         ch = 0;
	qwaq_event_t event = { };

	initialize ();
	init_pair (1, COLOR_WHITE, COLOR_BLUE);
	wbkgd (stdscr, COLOR_PAIR(1));
	wrefresh (stdscr);
	window_t win = create_window (20, 5, 50, 10);
	wbkgd (win, COLOR_PAIR(0));
	mvwprintf (win, 0, 0, "Hi there! (q to quit)");
	mvwprintf (win, 1, 1, "(?)Oo.");
	mvwprintf (win, 1, 2, "   \\_O>");
	mvwprintf (win, 1, 3, " %d %d", max_colors (), max_color_pairs ());
	wrefresh (win);
	do {
		if (get_event (&event)) {
			if (event.event_type == qe_key) {
				ch = event.e.key;
				mvwprintf (win, 1, 1, "key: %d", ch);
			} else if (event.event_type == qe_mouse) {
				mvwprintf (win, 1, 2, "mouse: %2d %2d %08x",
						   event.e.mouse.x,
						   event.e.mouse.y,
						   event.e.mouse.buttons);
			}
			wrefresh (win);
		}
	} while (ch != 'q' && ch != 'Q');
	destroy_window (win);
	get_event (&event);
	return 0;
}
