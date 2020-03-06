#ifndef __qwaq_textcontect_h
#define __qwaq_textcontect_h

#ifdef __QFCC__
#include <Object.h>
#include "qwaq-curses.h"
#include "qwaq-rect.h"

@interface TextContext : Object
{
	window_t    window;
}
+ (int) max_colors;
+ (int) max_color_pairs;
+ (void) init_pair: (int) pair, int fg, int bg;
+ (int) acs_char: (int) acs;
+ (void) move: (Point) pos;
+ (void) curs_set: (int) visibility;
+ (void) doupdate;

-init;
-initWithRect: (Rect) rect;
-initWithWindow: (window_t) window;
- (void) mvprintf: (Point) pos, string fmt, ...;
- (void) printf: (string) fmt, ...;
- (void) vprintf: (string) mft, @va_list args;
- (void) mvvprintf: (Point) pos, string mft, @va_list args;
- (void) refresh;
- (void) mvaddch: (Point) pos, int ch;
- (void) bkgd: (int) ch;
- (void) scrollok: (int) flag;
- (void) border: (box_sides_t) sides, box_corners_t corners;
@end

#else

#include "QF/pr_obj.h"

typedef struct qwaq_textcontext_s {
	pr_id_t     isa;
	pointer_t   window;
} qwaq_textcontext_t;

#endif

#endif//__qwaq_textcontect_h
