#include "draw.h"

qpic_t (string name, integer alpha) Draw_CachePic = #0;

void (integer x, integer y, qpic_t pic) Draw_Pic = #0;
void (integer x, integer y, qpic_t pic, integer srcx, integer srcy, integer width, integer height) Draw_SubPic = #0;
void (integer x, integer y, qpic_t pic) Draw_CenterPic = #0;

void (integer x, integer y, integer chr) Draw_Character = #0;
void (integer x, integer y, string text) Draw_String = #0;
void (integer x, integer y, string text, integer n) Draw_nString = #0;
void (integer x, integer y, string text) Draw_AltString = #0;
void (integer x, integer y, integer w, integer h, integer c) Draw_Fill = #0;

void (integer x, integer y, integer width, integer lines) text_box =
{
	local integer cx, cy, n;
	local qpic_t p;

	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp", 1);
	Draw_Pic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp", 1);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp", 1);
	Draw_Pic (cx, cy + 8, p);

	cx += 8;
	while (width > 0) {
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp", 1);
		Draw_Pic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp", 1);
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp", 1);
			Draw_Pic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp", 1);
		Draw_Pic (cx, cy + 8, p);
		width -= 2;
		cx += 16;
	}

	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp", 1);
	Draw_Pic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp", 1);
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_Pic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp", 1);
	Draw_Pic (cx, cy + 8, p);
};
