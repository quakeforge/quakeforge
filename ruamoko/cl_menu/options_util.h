#ifndef __options_util_h
#define __options_util_h

#include "gui/Group.h"

@class Text;
@class Slider;

@interface CvarObject : Object
{
	string name;
}
-(id)initWithCvar:(string)cvname;
@end

@interface CvarToggle : CvarObject
-(void)toggle;
-(BOOL)value;
@end

@interface CvarToggleView : Group
{
	Text title;
	Text value;
	CvarToggle toggle;
}
-(id)initWithBounds:(Rect)aRect title:(string)_title :(CvarToggle)_toggle;
-(void)toggle;
@end

@interface CvarColor : CvarObject
-(void)next;
-(void)prev;
-(integer)value;
@end

@interface CvarColorView : View
{
	CvarColor color;
}
-(id)initWithBounds:(Rect)aRect :(CvarColor)_color;
-(void)next;
-(void)prev;
@end

@interface CvarRange : CvarObject
{
	float min, max, step;
}
-(id)initWithCvar:(string)cvname min:(float)_min max:(float)_max step:(float)_step;
-(void)inc;
-(void)dec;
-(float)value;
-(integer)percentage;
@end

@interface CvarRangeView : Group
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

@interface CrosshairCvar : CvarObject
-(void) next;
-(integer) crosshair;
@end

@interface CrosshairView : View
{
	CrosshairCvar crosshair;
}
-(id)initWithBounds:(Rect)aRect :(CrosshairCvar)_crosshair;
-(void) next;
@end


@extern void (integer x, integer y) opt_cursor;
@extern void (integer x, integer y, integer spacing, string spacechar, string label, string valstr) draw_item;
@extern void (integer x, integer y, integer spacing, string label, string valstr) draw_val_item;
@extern integer (float min, float max, float val) to_percentage;
@extern float (float min, float max, float step, float val, integer cntflag) min_max_cnt;

#endif//__options_util_h
