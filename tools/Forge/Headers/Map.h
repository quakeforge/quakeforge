/*
	Map.h

	Map class definition for Forge

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

// Map is a collection of Entity objects

@interface Map: NSMutableArray
{
	id		currentEntity;
	id		oldselection;	// temp when loading a new map
	float	minz, maxz;
}

- (void) newMap;

- (void) writeStats;

- (void) readMapFile: (NSString *) fname;
- (void) writeMapFile: (NSString *) fname useRegion: (BOOL) reg;

- entityConnect: (vec3_t) p1 : (vec3_t) p2;

- selectRay: (vec3_t) p1 : (vec3_t) p2 : (BOOL) ef;
- grabRay: (vec3_t) p1 : (vec3_t) p2;
- setTextureRay: (vec3_t) p1 : (vec3_t) p2 : (BOOL) allsides;
- getTextureRay: (vec3_t) p1 : (vec3_t) p2;

- (id) currentEntity;
- (void) setCurrentEntity: (id) ent;

- (float) currentMaxZ;
- (float) currentMinZ;
- (void) setCurrentMaxZ: (float) m;
- (void) setCurrentMinZ: (float) m;

- (int) numSelected;
- (id) selectedBrush;			// returns the first selected brush

//
// operations on current selection
//
- makeSelectedPerform: (SEL) sel;
- makeUnselectedPerform: (SEL) sel;
- makeAllPerform: (SEL) sel;
- makeGlobalPerform: (SEL) sel;	// in and out of region

- cloneSelection: sender;

- makeEntity: sender;

- subtractSelection: sender;

- selectCompletelyInside: sender;
- selectPartiallyInside: sender;

- tallBrush: sender;
- shortBrush: sender;

- rotate_x: sender;
- rotate_y: sender;
- rotate_z: sender;

- flip_x: sender;
- flip_y: sender;
- flip_z: sender;

- selectCompleteEntity: sender;

@end

extern Map	*map;
