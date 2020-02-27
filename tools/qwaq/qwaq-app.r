#include "event.h"

typedef struct window_s *window_t;

void initialize (void) = #0;
window_t create_window (int xpos, int ypos, int xlen, int ylen) = #0;
void destroy_window (window_t win) = #0;
void wprintf (window_t win, string fmt, ...) = #0;
int wgetch (window_t win) = #0;

void process_input (void) = #0;
int get_event (qwaq_event_t *event) = #0;

int main (int argc, string *argv)
{
	int         ch = 0;
	qwaq_event_t event = { };

	initialize ();
	window_t win = create_window (20, 5, 50, 10);
	wprintf (win, "Hi there!\n");
	do {
		process_input ();

		if (get_event (&event)) {
			if (event.event_type == qe_key) {
				ch = event.e.key;
				wprintf (win, "key: %d\n", ch);
			} else if (event.event_type == qe_mouse) {
				wprintf (win, "mouse: %d %d %d %d %d\n",
						 event.e.mouse.id,
						 event.e.mouse.x,
						 event.e.mouse.y,
						 event.e.mouse.z,
						 event.e.mouse.buttons);
			}
		}
	} while (ch != 27);
	return 0;
}
