#ifndef __draw_h
#define __draw_h

struct _qpic_t = {
	integer width;
	integer height;
};
typedef _qpic_t [] qpic_t;

@extern qpic_t (string name, integer alpha) Draw_CachePic;

@extern void (integer x, integer y, qpic_t pic) Draw_Pic;
@extern void (integer x, integer y, qpic_t pic, integer srcx, integer srcy, integer width, integer height) Draw_SubPic;
@extern void (integer x, integer y, qpic_t pic) Draw_CenterPic;

@extern void (integer x, integer y, integer chr) Draw_Character;
@extern void (integer x, integer y, string text) Draw_String;
@extern void (integer x, integer y, string text, integer n) Draw_nString;
@extern void (integer x, integer y, string text) Draw_AltString;
@extern void (integer x, integer y, integer w, integer h, integer c) Draw_Fill;
@extern void (integer x, integer y, integer width, integer lines) text_box;

#endif//__draw_h
