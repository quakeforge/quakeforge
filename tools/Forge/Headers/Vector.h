/*
	Vector.h

	A vector math class supporting up to six dimensions (interface)

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

#include <QF/mathlib.h>

#import <Foundation/NSObject.h>

@interface Vector: NSObject <NSCopying>
{
	unsigned int	_numDimensions;
	double			_values[6];
}

- (id) initWithString: (NSString *) str;
- (id) initwithNumbers: ...;
- (id) zero dimensions: (unsigned char) d;
- (id) one dimensions: (unsigned char) d;

- (double) x;				// Convenience methods for three-dimensional vectors
- (double) y;
- (double) z;
- (double) dimension: (unsigned char) d;

- (void) setX: (double) val;
- (void) setY: (double) val;
- (void) setZ: (double) val;
- (void) setDimension: (unsigned char) d to: (double) val;

- (unsigned char) dimensions;

- (double) length;
- (double) direction;

- (void) add: (Vector *) vec;
- (void) subtract: (Vector *) vec;
- (void) scale: (double) scalar;
- (void) invert;			// Turn the direction around
- (void) normalize; 		// Scale the vector so that its length is 1

- (void) isEqualToVector: (Vector *) vec;

- (NSString *) stringValue;
@end

Vector *
CrossProduct (Vector *, Vector *);

double
DotProduct (Vector *, Vector *);
