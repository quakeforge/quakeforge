#ifndef __options_util_h
#define __options_util_h

#include "gui/Group.h"

@class Text;
@class Slider;

@interface CvarRange : Object
{
	string name;
	float min, max, step;
}
-(id)initWithCvar:(string)cvname min:(float)_min max:(float)_max step:(float)_step;
-(void)inc;
-(void)dec;
-(float)value;
-(integer)percentage;
@end

@interface RangeSlider : Group
{
	Text title;
	Text value;
	Slider slider;
	CvarRange range;
}
-(id)initWithBounds:(Rect)aRect title:(string)_title sliderWidth:(integer)width :(CvarRange)_range;
-(void)inc;
-(void)dec;
@end

@extern void (integer x, integer y) opt_cursor;
@extern void (integer x, integer y, integer spacing, string spacechar, string label, string valstr) draw_item;
@extern void (integer x, integer y, integer spacing, string label, string valstr) draw_val_item;
@extern integer (float min, float max, float val) to_percentage;
@extern float (float min, float max, float step, float val, integer cntflag) min_max_cnt;

#endif//__options_util_h
