/*
    options_util.qc

    Utilities for the options menu

    Copyright (C) 2002 Robin Redeker <elmex@x-paste.de>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this program; if not, write to:

        Free Software Foundation, Inc.
        59 Temple Place - Suite 330
        Boston, MA  02111-1307, USA
*/

#include "cvar.h"
#include "draw.h"
#include "system.h"
#include "string.h"

#include "gui/Text.h"
#include "gui/Rect.h"
#include "gui/Slider.h"
#include "options_util.h"

@implementation CvarObject
-(id)init
{
	self = [super init];
	name = str_new ();
	return self;
}

-(id)initWithCvar:(string)cvname
{
	self = [self init];
	str_copy (name, cvname);
	return self;
}

-(void)dealloc
{
	str_free (name);
}
@end

@implementation CvarToggle

-(void)toggle
{
	Cvar_Toggle (name);
}

-(BOOL)value
{
	return Cvar_GetInteger (name);
}

@end

@implementation CvarToggleView

-(void)update
{
	[value setText:[toggle value] ? "On" : "Off"];
}

-(id)initWithBounds:(Rect)aRect title:(string)_title :(CvarToggle)_toggle
{
	local Rect rect;

	self = [super initWithBounds:aRect];

	toggle = _toggle;
	
	rect = [[Rect alloc] initWithComponents:0 :0 :strlen (_title) * 8 :8];
	title = [[Text alloc] initWithBounds:rect text:_title];

	rect.size.width = 3 * 8;
	rect.origin.x = xlen - rect.size.width;
	value = [[Text alloc] initWithBounds:rect];

	[self addView:title];
	[self addView:value];

	[self update];

	return self;
}

-(void)toggle
{
	[toggle toggle];
	[self update];
}

@end

@implementation CvarColor
-(void)next
{
	local float val = Cvar_GetFloat (name);
	val = min_max_cnt (0, 13, 1, val, 1);
	Cvar_SetFloat (name, val);
}

-(void)prev
{
	local float val = Cvar_GetFloat (name);
	val = min_max_cnt (0, 13, 1, val, 0);
	Cvar_SetFloat (name, val);
}

-(integer)value
{
	return Cvar_GetInteger (name);
}
@end

@implementation CvarColorView
-(id)initWithBounds:(Rect)aRect :(CvarColor)_color
{
	self = [self initWithBounds:aRect];
	color = _color;
	return self;
}

-(void)next
{
	[color next];
}

-(void)prev
{
	[color prev];
}

-(void)draw
{
	local integer xl;
	xl = xlen / 8 - 2;
	text_box (xabs, yabs, xl, ylen / 8 - 2);
	xl = (xl + 1) & ~1;		// text_box only does multiples of 2
	Draw_Fill (xabs + 8, yabs + 8, xl * 8, ylen - 16,
			   [color value] * 16 + 8);
}
@end

@implementation CvarRange

-(id)initWithCvar:(string)cvname min:(float)_min max:(float)_max step:(float)_step
{
	self = [super init];

	name = str_new ();
	str_copy (name, cvname);
	min = _min;
	max = _max;
	step = _step;

	return self;
}

-(void)inc
{
	local float val = Cvar_GetFloat (name);
	val = min_max_cnt (min, max, step, val, 1);
	Cvar_SetFloat (name, val);
}

-(void)dec
{
	local float val = Cvar_GetFloat (name);
	val = min_max_cnt (min, max, step, val, 0);
	Cvar_SetFloat (name, val);
}

-(float)value
{
	return Cvar_GetFloat (name);
}

-(integer)percentage
{
	return to_percentage(min, max, Cvar_GetFloat (name));
}

@end

@implementation CvarRangeView

-(void)update
{
	[slider setIndex:[range percentage]];
	[value setText:ftos ([range value])];
}

-(id)initWithBounds:(Rect)aRect title:(string)_title sliderWidth:(integer)width :(CvarRange)_range
{
	local Rect rect;

	self = [super initWithBounds:aRect];

	range = _range;
	
	rect = [[Rect alloc] initWithComponents:0 :0 :strlen (_title) * 8 :8];
	title = [[Text alloc] initWithBounds:rect text:_title];

	rect.origin.x += rect.size.width + 8;
	rect.size.width = width;
	if (rect.origin.x + rect.size.width > xlen)
		rect.size.width = xlen - rect.origin.x;
	slider = [[Slider alloc] initWithBounds:rect size:100];

	rect.origin.x += rect.size.width + 8;
	rect.size.width = xlen - rect.origin.x;
	value = [[Text alloc] initWithBounds:rect];

	[self addView:title];
	[self addView:slider];
	[self addView:value];

	[self update];

	return self;
}

-(void)inc
{
	[range inc];
	[self update];
}

-(void)dec
{
	[range dec];
	[self update];
}

@end

@implementation CrosshairCvar
-(void) next
{
	local integer val = Cvar_GetInteger (name);
	Cvar_SetInteger (name, (val + 1) % 4);
}

-(integer) crosshair
{
	return Cvar_GetInteger (name);
}
@end

@implementation CrosshairView
{
	CrosshairCvar crosshair;
}

-(id)initWithBounds:(Rect)aRect :(CrosshairCvar)_crosshair
{
	self = [self initWithBounds:aRect];
	crosshair = _crosshair;
	return self;
}

-(void) next
{
	[crosshair next];
}

-(void) draw
{
	Draw_Fill (xabs, yabs, xlen, ylen, 0);
	Draw_Crosshair ([crosshair crosshair], xabs + xlen / 2, yabs + ylen / 2);
}
@end

/*
	opt_cursor

	function for drawing the cursor
*/
void (integer x, integer y)
opt_cursor =
{
	// use time becaus we want a nice rotaing cursor
	Draw_Character (x, y, 12 + (integer) (time * 4) & 1);
};

/*
	draw_item
 
	Draws a item with a specific spacing between label and value to
	position x, y.
	Used as helper function for draw_val_item.
*/
void (integer x, integer y, integer spacing, string spacechar,
	  string label, string valstr)
draw_item =
{
	local integer	i;

	Draw_String	(x,  y,	label); 
	for (i = x + (integer) strlen (label) * 8; i < (x+spacing); i += 8) {
		Draw_String (i, y, spacechar);
	}
	Draw_String (x + spacing,  y, valstr);
};

/* 
	draw_val_item

	Draws a nice menu item. 
	Use this function for a consistent look of menu items!
	Example:
		<Label>.....:<valstr>
			spacing are the number of the points to put
*/
void (integer x, integer y, integer spacing, string label, string valstr) 
draw_val_item =
{
	draw_item (x, y, spacing, ".", label, ":" + valstr);
};

/*
	to_percentage

	Calculates the percentage of a value relative
	to a min and max value.
*/
integer (float min, float max, float val) 
to_percentage =
{
	local float		max_v = (max - min);
	local integer	perc;

	val -= min;

	if (val > max_v) {
		val = max_v;
	}
	if (val < 0) {
		val = 0;
	}
	perc = (integer) ((val / max_v) * 100);

	return perc;
};

/*
	min_max_cnt

	Increases or decreases a value by take care of the bordervalues.
	min, max are the borders.
	step is the step by in-/de-creasing.
	cntflag should be true for increasing and false for decreasing
*/
float (float min, float max, float step, float val, integer cntflag)
min_max_cnt =
{
	if (cntflag) {
		val += step;
	} else {
		val -= step;
	}

	if (val > max) {
		val = max;
	} else if (val < min) {
		val = min;
	}
	return val;
};
