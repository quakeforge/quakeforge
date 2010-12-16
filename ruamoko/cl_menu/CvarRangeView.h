#ifndef __CvarRangeView_h
#define __CvarRangeView_h

#include "gui/Group.h"

@class Text;
@class Slider;
@class CvarRange;

@interface CvarRangeView : Group
{
	Text title;
	Text value;
	Slider slider;
	CvarRange range;
}
-(void)update;
-(id)initWithBounds:(Rect)aRect title:(string)_title sliderWidth:(integer)width :(CvarRange)_range;
-(void)inc;
-(void)dec;
@end

#endif//__CvarRangeView_h
