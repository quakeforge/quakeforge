#ifndef __qwaq_curses_h
#define __qwaq_curses_h

#include "event.h"

typedef struct box_sides_s {
	int         ls;
	int         rs;
	int         ts;
	int         bs;
} box_sides_t;

typedef struct box_corners_s {
	int         tl;
	int         tr;
	int         bl;
	int         br;
} box_corners_t;

#ifdef __QFCC__
#include "qwaq-rect.h"

// names, order and comments lifted from ncurses.h
typedef enum {
/* VT100 symbols begin here */
	ACS_ULCORNER = 256,	/* upper left corner */
	ACS_LLCORNER,		/* lower left corner */
	ACS_URCORNER,		/* upper right corner */
	ACS_LRCORNER,		/* lower right corner */
	ACS_LTEE,			/* tee pointing right */
	ACS_RTEE,			/* tee pointing left */
	ACS_BTEE,			/* tee pointing up */
	ACS_TTEE,			/* tee pointing down */
	ACS_HLINE,			/* horizontal line */
	ACS_VLINE,			/* vertical line */
	ACS_PLUS,			/* large plus or crossover */
	ACS_S1,				/* scan line 1 */
	ACS_S9,				/* scan line 9 */
	ACS_DIAMOND,		/* diamond */
	ACS_CKBOARD,		/* checker board (stipple) */
	ACS_DEGREE,			/* degree symbol */
	ACS_PLMINUS,		/* plus/minus */
	ACS_BULLET,			/* bullet */
/* Teletype 5410v1 symbols begin here */
	ACS_LARROW,			/* arrow pointing left */
	ACS_RARROW,			/* arrow pointing right */
	ACS_DARROW,			/* arrow pointing down */
	ACS_UARROW,			/* arrow pointing up */
	ACS_BOARD,			/* board of squares */
	ACS_LANTERN,		/* lantern symbol */
	ACS_BLOCK,			/* solid square block */
/*
 * These aren't documented, but a lot of System Vs have them anyway
 * (you can spot pprryyzz{{||}} in a lot of AT&T terminfo strings).
 * The ACS_names may not match AT&T's, our source didn't know them.
 */
	ACS_S3,				/* scan line 3 */
	ACS_S7,				/* scan line 7 */
	ACS_LEQUAL,			/* less/equal */
	ACS_GEQUAL,			/* greater/equal */
	ACS_PI,				/* Pi */
	ACS_NEQUAL,			/* not equal */
	ACS_STERLING,		/* UK pound sign */

/*
 * Line drawing ACS names are of the form ACS_trbl, where t is the top, r
 * is the right, b is the bottom, and l is the left.  t, r, b, and l might
 * be B (blank), S (single), D (double), or T (thick).  The subset defined
 * here only uses B and S.
 */
	ACS_BSSB = ACS_ULCORNER,
	ACS_SSBB = ACS_LLCORNER,
	ACS_BBSS = ACS_URCORNER,
	ACS_SBBS = ACS_LRCORNER,
	ACS_SBSS = ACS_RTEE,
	ACS_SSSB = ACS_LTEE,
	ACS_SSBS = ACS_BTEE,
	ACS_BSSS = ACS_TTEE,
	ACS_BSBS = ACS_HLINE,
	ACS_SBSB = ACS_VLINE,
	ACS_SSSS = ACS_PLUS,
} qwaq_acs_chars;

typedef struct window_s *window_t;
typedef struct panel_s *panel_t;

extern window_t stdscr;

void initialize (void);
window_t create_window (int xpos, int ypos, int xlen, int ylen);
void destroy_window (window_t win);
void mvwprintf (window_t win, int x, int y, string fmt, ...);
void wprintf (window_t win, string fmt, ...);
void wvprintf (window_t win, string fmt, @va_list args);
void mvwvprintf (window_t win, int x, int y, string fmt, @va_list args);
void wrefresh (window_t win);
void mvwaddch (window_t win, int x, int y, int ch);
void waddch (window_t win, int ch);
void mvwaddstr (window_t win, int x, int y, string str);
void waddstr (window_t win, string str);
void mvwhline (window_t win, int x, int y, int ch, int n);

panel_t create_panel (window_t window);
void destroy_panel (panel_t panel);
void hide_panel (panel_t panel);
void show_panel (panel_t panel);
void top_panel (panel_t panel);
void bottom_panel (panel_t panel);
void move_panel (panel_t panel, int x, int y);
window_t panel_window (panel_t panel);
void update_panels (void);
void replace_panel (panel_t panel, window_t window);
void doupdate (void);

int get_event (qwaq_event_t *event);
int max_colors (void);
int max_color_pairs (void);
int init_pair (int pair, int f, int b);
void wbkgd (window_t win, int ch);
void werase (window_t win);
void scrollok (window_t win, int flag);

int acs_char (int acs);
int curs_set (int visibility);
int move (int x, int y);

void wborder (window_t window, box_sides_t sides, box_corners_t corners);
void mvwblit_line (window_t window, int x, int y, int *wch, int len);
void wresize (window_t window, int width, int height);
void resizeterm (int width, int height);

Rect getwrect (struct window_s *window);

void printf(string fmt, ...);
// qfcc stuff
#else
// gcc stuff

#include <curses.h>
#include <panel.h>

#include "QF/dstring.h"
#include "QF/progs.h"
#include "QF/ringbuffer.h"

#define QUEUE_SIZE 16
#define STRING_ID_QUEUE_SIZE 8		// must be > 1
#define COMMAND_QUEUE_SIZE 1280

typedef struct window_s {
	WINDOW     *win;
} window_t;

typedef struct panel_s {
	PANEL      *panel;
	int         window_id;
} panel_t;

typedef struct rwcond_s {
	pthread_cond_t rcond;
	pthread_cond_t wcond;
	pthread_mutex_t mut;
} rwcond_t;

typedef enum {
	esc_ground,
	esc_escape,
	esc_csi,
	esc_mouse,
	esc_sgr,
	esc_key,
} esc_state_t;

typedef struct qwaq_resources_s {
	progs_t    *pr;
	int         initialized;
	window_t    stdscr;
	PR_RESMAP (window_t) window_map;
	PR_RESMAP (panel_t) panel_map;
	rwcond_t    event_cond;
	RING_BUFFER (qwaq_event_t, QUEUE_SIZE) event_queue;
	rwcond_t    command_cond;
	RING_BUFFER (int, COMMAND_QUEUE_SIZE) command_queue;
	rwcond_t    results_cond;
	RING_BUFFER (int, COMMAND_QUEUE_SIZE) results;
	rwcond_t    string_id_cond;
	RING_BUFFER (int, STRING_ID_QUEUE_SIZE) string_ids;
	dstring_t   strings[STRING_ID_QUEUE_SIZE - 1];

	dstring_t   escbuff;
	esc_state_t escstate;
	unsigned    button_state;
	int         mouse_x;
	int         mouse_y;
	qwaq_event_t lastClick;
	struct hashtab_s *key_sequences;
} qwaq_resources_t;
// gcc stuff

void qwaq_input_init (qwaq_resources_t *res);
void qwaq_input_shutdown (qwaq_resources_t *res);
void qwaq_process_input (qwaq_resources_t *res);
void qwaq_init_timeout (struct timespec *timeout, long time);
int qwaq_add_event (qwaq_resources_t *res, qwaq_event_t *event);
void qwaq_init_cond (rwcond_t *cond);
#endif

#endif//__qwaq_curses_h
