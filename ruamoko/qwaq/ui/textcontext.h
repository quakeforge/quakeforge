#ifndef __qwaq_ui_textcontect_h
#define __qwaq_ui_textcontect_h

#ifdef __QFCC__
#include <Object.h>
#include "ruamoko/qwaq/ui/curses.h"
#include "ruamoko/qwaq/ui/draw.h"
#include "ruamoko/qwaq/ui/rect.h"

@class DrawBuffer;

@interface TextContext : Object<TextContext>
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
	int         background;
}
+ (int) max_colors;
+ (int) max_color_pairs;
+ (void) init_pair: (int) pair, int fg, int bg;
+ (int) acs_char: (int) acs;
+ (void) move: (Point) pos;
+ (void) curs_set: (int) visibility;
+ (void) doupdate;
+ (TextContext *) screen;

+(TextContext *)textContext;
+(TextContext *)withRect:(Rect)rect;
+(TextContext *)withWindow:(window_t)window;

-init;
-initWithRect: (Rect) rect;
-initWithWindow: (window_t) window;

- (window_t) window;
- (Extent) size;

- blitFromBuffer: (DrawBuffer *) srcBuffer to: (Point) pos from: (Rect) rect;

- (void) refresh;
+ (void) refresh;
- (void) bkgd: (int) ch;
- (void) scrollok: (int) flag;
- (void) border: (box_sides_t) sides, box_corners_t corners;
- (void) mvhline: (Point) pos, int ch, int n;
- (void) mvvline: (Point) pos, int ch, int n;
-clearReact: (Rect) rect;
@end

#else

#include "QF/pr_obj.h"

typedef struct qwaq_textcontext_s {
	pr_id_t     isa;
	pointer_t   window;
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
	int         background;
} qwaq_textcontext_t;

#endif

#endif//__qwaq_ui_textcontect_h
