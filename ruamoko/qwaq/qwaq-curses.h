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

@extern window_t stdscr;

@extern void initialize (void);
@extern window_t create_window (int xpos, int ypos, int xlen, int ylen);
@extern void destroy_window (window_t win);
@extern void mvwprintf (window_t win, int x, int y, string fmt, ...);
@extern void wprintf (window_t win, string fmt, ...);
@extern void wvprintf (window_t win, string fmt, @va_list args);
@extern void mvwvprintf (window_t win, int x, int y,
						 string fmt, @va_list args);
@extern void wrefresh (window_t win);
@extern void mvwaddch (window_t win, int x, int y, int ch);
@extern void waddch (window_t win, int ch);

@extern panel_t create_panel (window_t window);
@extern void destroy_panel (panel_t panel);
@extern void hide_panel (panel_t panel);
@extern void show_panel (panel_t panel);
@extern void top_panel (panel_t panel);
@extern void bottom_panel (panel_t panel);
@extern void move_panel (panel_t panel, int x, int y);
@extern window_t panel_window (panel_t panel);
@extern void update_panels (void);
@extern void doupdate (void);

@extern int get_event (qwaq_event_t *event);
@extern int max_colors (void);
@extern int max_color_pairs (void);
@extern int init_pair (int pair, int f, int b);
@extern void wbkgd (window_t win, int ch);
@extern void scrollok (window_t win, int flag);

@extern int acs_char (int acs);
@extern int curs_set (int visibility);
@extern int move (int x, int y);

@extern void wborder (window_t window,
					  box_sides_t sides, box_corners_t corners);
@extern void mvwblit_line (window_t window, int x, int y, int *wch, int len);
#endif

#endif//__qwaq_curses_h
