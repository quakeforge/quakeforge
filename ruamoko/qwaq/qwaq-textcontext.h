#ifndef __qwaq_textcontect_h
#define __qwaq_textcontect_h

#ifdef __QFCC__
#include <Object.h>
#include "qwaq-curses.h"
#include "qwaq-rect.h"

@class DrawBuffer;

@interface TextContext : Object
{
	window_t    window;
	union {
		Rect        rect;
		struct {
			Point       offset;
			Extent      size;
		};
		struct {
			int         xpos;
			int         ypos;
			int         xlen;
			int         ylen;
		};
	};
}
+ (int) max_colors;
+ (int) max_color_pairs;
+ (void) init_pair: (int) pair, int fg, int bg;
+ (int) acs_char: (int) acs;
+ (void) move: (Point) pos;
+ (void) curs_set: (int) visibility;
+ (void) doupdate;
+ (TextContext *) screen;

-init;
-initWithRect: (Rect) rect;
-initWithWindow: (window_t) window;

- (window_t) window;
- (Extent) size;

- blitFromBuffer: (DrawBuffer *) srcBuffer to: (Point) pos from: (Rect) rect;

- (void) printf: (string) fmt, ...;
- (void) vprintf: (string) mft, @va_list args;
- (void) addch: (int) ch;
- (void) mvprintf: (Point) pos, string fmt, ...;
- (void) mvvprintf: (Point) pos, string mft, @va_list args;
- (void) mvaddch: (Point) pos, int ch;
- (void) refresh;
+ (void) refresh;
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
