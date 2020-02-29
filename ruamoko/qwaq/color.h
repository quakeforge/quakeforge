#ifndef __qwaq_color_h
#define __qwaq_color_h

#ifndef COLOR_PAIR
// double protection in case this header is included in a C file

#define COLOR_PAIR(cp) ((cp) << 8)

// taken from ncurses.h
#define COLOR_BLACK		0
#define COLOR_RED		1
#define COLOR_GREEN		2
#define COLOR_YELLOW	3
#define COLOR_BLUE		4
#define COLOR_MAGENTA	5
#define COLOR_CYAN		6
#define COLOR_WHITE		7

#endif

#endif//__qwaq_color_h
