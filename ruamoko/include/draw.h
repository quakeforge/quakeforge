#ifndef __ruamoko_draw_h
#define __ruamoko_draw_h

#include <Object.h>

struct _qpic_t {
	int width;
	int height;
	int handle;
};
typedef struct _qpic_t *qpic_t;

@extern void Draw_FreePic (qpic_t pic);
@extern qpic_t Draw_MakePic (int width, int heiight, string data);
@extern qpic_t Draw_CachePic (string name, int alpha);

@extern void Draw_Pic (int x, int y, qpic_t pic);
@extern void Draw_Picf (float x, float y, qpic_t pic);
@extern void Draw_SubPic (int x, int y, qpic_t pic, int srcx, int srcy, int width, int height);
@extern void Draw_CenterPic (int x, int y, qpic_t pic);

@extern void Draw_Character (int x, int y, int chr);
@extern void Draw_String (int x, int y, string text);
@extern void Draw_nString (int x, int y, string text, int n);
@extern void Draw_AltString (int x, int y, string text);
@extern void Draw_Fill (int x, int y, int w, int h, int c);
@extern void Draw_Crosshair (int ch, int x, int y);
@extern void text_box (int x, int y, int width, int lines);

@interface QPic : Object
{
	string	name;
	struct _qpic_t size;
	BOOL	centered;
}
-initName:(string)n;
-initName:(string)n Centered:(BOOL)c;
-draw:(int)x :(int)y;
-draw:(int)x :(int)y :(int)srcx :(int)srcy :(int)width :(int)height;
-(int)width;
-(int)height;
@end

#endif//__ruamoko_draw_h
