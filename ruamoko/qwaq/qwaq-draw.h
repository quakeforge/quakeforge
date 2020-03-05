#ifndef __qwaq_draw_h
#define __qwaq_draw_h

@protocol Draw
-draw;
-redraw;
-setOwner: owner;
-(struct window_s*) getWindow;
@end

#endif
