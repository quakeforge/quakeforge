#ifndef __ruamoko_draw_h
#define __ruamoko_draw_h

#include "Object.h"

struct _qpic_t {
	integer width;
	integer height;
};
typedef struct _qpic_t [] qpic_t;

@extern qpic_t (string name, integer alpha) Draw_CachePic;

@extern void (integer x, integer y, qpic_t pic) Draw_Pic;
@extern void (integer x, integer y, qpic_t pic, integer srcx, integer srcy, integer width, integer height) Draw_SubPic;
@extern void (integer x, integer y, qpic_t pic) Draw_CenterPic;

@extern void (integer x, integer y, integer chr) Draw_Character;
@extern void (integer x, integer y, string text) Draw_String;
@extern void (integer x, integer y, string text, integer n) Draw_nString;
@extern void (integer x, integer y, string text) Draw_AltString;
@extern void (integer x, integer y, integer w, integer h, integer c) Draw_Fill;
@extern void (integer ch, integer x, integer y) Draw_Crosshair;
@extern void (integer x, integer y, integer width, integer lines) text_box;

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
