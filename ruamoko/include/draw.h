#ifndef __ruamoko_draw_h
#define __ruamoko_draw_h

#include "Object.h"

struct _qpic_t {
	integer width;
	integer height;
};
typedef struct _qpic_t *qpic_t;

@extern qpic_t Draw_CachePic (string name, integer alpha);

@extern void Draw_Pic (integer x, integer y, qpic_t pic);
@extern void Draw_SubPic (integer x, integer y, qpic_t pic, integer srcx, integer srcy, integer width, integer height);
@extern void Draw_CenterPic (integer x, integer y, qpic_t pic);

@extern void Draw_Character (integer x, integer y, integer chr);
@extern void Draw_String (integer x, integer y, string text);
@extern void Draw_nString (integer x, integer y, string text, integer n);
@extern void Draw_AltString (integer x, integer y, string text);
@extern void Draw_Fill (integer x, integer y, integer w, integer h, integer c);
@extern void Draw_Crosshair (integer ch, integer x, integer y);
@extern void text_box (integer x, integer y, integer width, integer lines);

@interface QPic : Object
{
	string	name;
	struct _qpic_t size;
	BOOL	centered;
}
-initName:(string)n;
-initName:(string)n Centered:(BOOL)c;
-draw:(integer)x :(integer)y;
-draw:(integer)x :(integer)y :(integer)srcx :(integer)srcy :(integer)width :(integer)height;
-(integer)width;
-(integer)height;
@end

#endif//__ruamoko_draw_h
