/*
	Brush.h

	(description)

	Copyright (C) 2001 Jeff Teunissen <deek@d2dc.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/
#ifdef HAVE_CONFIG_H
# include "Config.h"
#endif

#import <appkit/appkit.h>
#import "SetBrush.h"
#import "EditWindow.h"

extern	id	brush_i;

extern	BOOL	brushdraw;			// YES when drawing cutbrushes and ents

@interface Brush : SetBrush
{
	id			cutbrushes_i;
	id			cutentities_i;
	boolean		updatemask[MAXBRUSHVERTEX];
	BOOL		dontdraw;				// for modal instance loops	
	BOOL		deleted;				// when not visible at all	
}

- init;

- initFromSetBrush: br;

- deselect;
- (BOOL)isSelected;

- (BOOL)XYmouseDown: (NSPoint *)pt;		// return YES if brush handled
- (BOOL)ZmouseDown: (NSPoint *)pt;		// return YES if brush handled

- _keyDown:(NSEvent *)theEvent;

- (NSPoint)centerPoint;						// for camera flyby mode

- InstanceSize;
- XYDrawSelf;
- ZDrawSelf;
- CameraDrawSelf;

- flipHorizontal: sender;
- flipVertical: sender;
- rotate90: sender;

- makeTall: sender;
- makeShort: sender;
- makeWide: sender;
- makeNarrow: sender;

- placeEntity: sender;

- cut: sender;
- copy: sender;

- addBrush;

@end


