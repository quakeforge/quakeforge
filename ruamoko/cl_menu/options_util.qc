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

#include "draw.h"
#include "system.h"
#include "string.h"

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
