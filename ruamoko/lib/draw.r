#include <draw.h>

void Draw_FreePic (qpic_t pic) = #0;
qpic_t Draw_MakePic (int width, int heiight, string data) = #0;
qpic_t (string name, int alpha) Draw_CachePic = #0;

void (int x, int y, qpic_t pic) Draw_Pic = #0;
void (float x, float y, qpic_t pic) Draw_Picf = #0;
void (int x, int y, qpic_t pic, int srcx, int srcy, int width, int height) Draw_SubPic = #0;
void (int x, int y, qpic_t pic) Draw_CenterPic = #0;

void (int x, int y, int chr) Draw_Character = #0;
void (int x, int y, string text) Draw_String = #0;
void (int x, int y, string text, int n) Draw_nString = #0;
void (int x, int y, string text) Draw_AltString = #0;
void (int x, int y, int w, int h, int c) Draw_Fill = #0;
void (int ch, int x, int y) Draw_Crosshair = #0;

void (int x, int y, int width, int lines) text_box =
{
	local int cx, cy, n;
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

@implementation QPic

-initName:(string)n Centered:(BOOL)c
{
	[super init];
	name = n;
	local qpic_t pic = Draw_CachePic (name, 1);
	size.width = pic.width;
	size.height = pic.height;
	return self;
}

-initName:(string)n
{
	return [self initName:n Centered:NO];
}

-draw:(int)x :(int)y
{
	local qpic_t pic = Draw_CachePic (name, 1);
	Draw_Pic (x, y, pic);
	return self;
}

-draw:(int)x :(int)y :(int)srcx :(int)srcy :(int)width :(int)height
{
	local qpic_t pic = Draw_CachePic (name, 1);
	Draw_SubPic (x, y, pic, srcx, srcy, width, height);
	return self;
}

-(int)width
{
	return size.width;
}

-(int)height
{
	return size.height;
}

@end
