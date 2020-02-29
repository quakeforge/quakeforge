#include "event.h"

typedef struct window_s *window_t;

void initialize (void) = #0;
window_t create_window (int xpos, int ypos, int xlen, int ylen) = #0;
void destroy_window (window_t win) = #0;
void mvwprintf (window_t win, int x, int y, string fmt, ...) = #0;

int get_event (qwaq_event_t *event) = #0;

int main (int argc, string *argv)
{
	int         ch = 0;
	qwaq_event_t event = { };

	initialize ();
	window_t win = create_window (20, 5, 50, 10);
	mvwprintf (win, 0, 0, "Hi there! (q to quit)\n");
	mvwprintf (win, 1, 1, "(?)Oo.\n");
	mvwprintf (win, 1, 2, "   \\_O>\n");
	do {
		if (get_event (&event)) {
			if (event.event_type == qe_key) {
				ch = event.e.key;
				mvwprintf (win, 1, 1, "key: %d\n", ch);
			} else if (event.event_type == qe_mouse) {
				mvwprintf (win, 1, 2, "mouse: %2d %2d %08x\n",
						   event.e.mouse.x,
						   event.e.mouse.y,
						   event.e.mouse.buttons);
			}
		}
	} while (ch != 'q' && ch != 'Q');
	destroy_window (win);
	get_event (&event);
	return 0;
}
