#ifndef __qwaq_curses_h
#define __qwaq_curses_h

#include "event.h"

#ifdef __QFCC__
typedef struct window_s *window_t;
typedef struct panel_s *panel_t;

@extern window_t stdscr;

@extern void initialize (void);
@extern window_t create_window (int xpos, int ypos, int xlen, int ylen);
@extern void destroy_window (window_t win);
@extern void mvwprintf (window_t win, int x, int y, string fmt, ...);
@extern void wprintf (window_t win, string fmt, ...);
@extern void wrefresh (window_t win);

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
#endif

#endif//__qwaq_curses_h
