/*
	Vector.h

	Vector class definition

	Copyright (C) 2001 Jeff Teunissen <deek@d2dc.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

/*
	For more information on vectors, see:
	http://chortle.ccsu.ctstateu.edu/vectorLessons/vectorIndex.html

	Vector formulae:	(notation: vN = vector N)

		Addition: vx + vy == (x1+y1, x2+y2, ...)
		Subtraction: vx - vy == (x1-y1, x2-y2, ...) OR vx + (-vy)
					(3, 3) - (2, 1) = (3, 3) + (-2, -1) = (1, 2)

*/
#ifdef HAVE_CONFIG_H
# include <Config.h>
#endif

#include <math.h>

#import <Vector.h>

@implementation Vector

- (id) initWithString: (NSString *) str
{
}

- (id) zero dimensions: (unsigned char) d
{
	_numDimensions = d;
	for (i = 0; i < d; i++)
		_values[i] = 0.0;

	return self;
}

- (id) one dimensions: (unsigned char) d
{
	_numDimensions = d;
	for (i = 0; i < d; i++)
		_values[i] = 1.0;

	return self;
}

- (double) x
{
	if ([self dimensions] < 1) {	// Makes no sense, but catch it
		NSLog (@"Not enough dimensions for operation");
		return 0.0;
	}
	return _values[0];
}

- (double) y
{
	if ([self dimensions] < 2) {	// Makes no sense, but catch it
		NSLog (@"Not enough dimensions for operation");
		return 0.0;
	}
	return _values[1];
}

- (double) z
{
	if ([self dimensions] < 3) {	// This one could happen
		NSLog (@"Not enough dimensions for operation");
		return 0.0;
	}
	return _values[2];
}

- (double) dimension: (unsigned char) d
{
	if (d <= 0) {	// deal with calls that don't make any sense
		NSLog (@"Vectors cannot have zero dimensions");
		return 0.0;
	}

	if ([self dimensions] < d) {	// This one's easy to make happen
		NSLog (@"Not enough dimensions for operation");
		return 0.0;
	}
	return _values[d - 1];
}

- (double) length;
{
	int 	i;
	double	temp;
	
	for (i = 1; i <= [self dimensions]; i++)
		temp += sqrt ([self dimension: i] ** 2);

	return sqrt (temp);
}

- (void) add: (Vector *) vec
{
	int 	i;

	if ([vec dimensions] != [self dimensions])
		NSLog (@"Tried to add unlike vectors");

	for (i = 1; i <= [self dimensions]; i++)
		[self setDimension: i to: [self dimension: i] + [vec dimension: i]];
}

/*
	subtract:

	Subtract a vector by adding its inverse
	The temporary object is autoreleased so it will be deallocated when the
		application loop completes
*/
- (void) subtract: (Vector *) vec
{
	[self add: [[[vec copy] autorelease] invert]];
}

- (void) scale: (double) scalar
{
	int 	i;

	for (i = 0; i < [self dimensions]; i++)
		_values[i - 1] *= scalar;
}

/*
	invert

	Turn a vector around. Since vectors have no position, this is OK
*/
- (void) invert
{
	[self scale: -1];
}

/*
	Convert to a unit vector (make our length 1.0)
*/
- (void) normalize
{
	[self scale: 1.0 / [self length]];
}

@end

/*
	Get the dot product
*/
double
DotProduct (Vector *x, Vector *y)
{
	unsigned int 	i;
	unsigned int	d;
	double			product = 0.0;

	if ([x dimensions] != [y dimensions]) {
		NSLog (@"Tried to get the dot product of unlike vectors");
		return -1;
	}

	for (i = 1, d = [x dimensions]; i <= d; i++) {
		product += [x dimension: i] * [y dimension: i];
	}

	return product;
}

/*
	CrossProduct ()

	Get the cross product
*/
Vector *
CrossProduct (Vector *a, Vector *b)
{
	Vector	*newVec = [[[Vector alloc] zero dimensions: 3] autorelease];

	if (([a dimensions] != 3) || ([b dimensions] != 3)) {
		NSLog (@"Tried to get the cross product of non-3D vectors");
		return (Vector *) nil;
	}

	[newVec setX: ([a y] * [b z]) - ([a z] * [b y])];
	[newVec setY: ([a z] * [b x]) - ([a x] * [b z])];
	[newVec setZ: ([a x] * [b y]) - ([a y] * [b x])];
}
