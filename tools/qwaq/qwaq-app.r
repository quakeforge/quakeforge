typedef struct window_s *window_t;

void initialize (void) = #0;
window_t create_window (int xpos, int ypos, int xlen, int ylen) = #0;
void destroy_window (window_t win) = #0;
void wprintf (window_t win, string fmt, ...) = #0;
int wgetch (window_t win) = #0;

int main (int argc, string *argv)
{
	int ch;

	initialize ();
	window_t win = create_window (20, 5, 50, 10);
	wprintf (win, "Hi there!\n");
	do {
		ch = wgetch (win);
		if (ch) {
			wprintf (win, "%d\n", ch);
		}
	} while (ch != 27);
	return 0;
}
