#ifndef Map_h
#define Map_h

#include <AppKit/AppKit.h>

#include "QF/mathlib.h"

// Map is a list of Entity objects

extern id  map_i;

@interface Map: NSMutableArray
{
	NSMutableArray  *array;
	id              currentEntity;
	id              oldselection;   // temp when loading a new map
	float           minz, maxz;
}

- (id) newMap;

- (id) writeStats;

- (id) readMapFile: (const char *)fname;
- (id) writeMapFile: (const char *)fname useRegion: (BOOL)reg;

- (id) entityConnect: (vec3_t)p1: (vec3_t)p2;

- (id) selectRay: (vec3_t)p1: (vec3_t)p2: (BOOL)ef;
- (id) grabRay: (vec3_t)p1: (vec3_t)p2;
- (id) setTextureRay: (vec3_t)p1: (vec3_t)p2: (BOOL)allsides;
- (id) getTextureRay: (vec3_t)p1: (vec3_t)p2;

- (id) currentEntity;
- (id) setCurrentEntity: ent;

- (float) currentMinZ;
- (id) setCurrentMinZ: (float)m;
- (float) currentMaxZ;
- (id) setCurrentMaxZ: (float)m;

- (int) numSelected;
- (id) selectedBrush;               // returns the first selected brush

//
// operations on current selection
//
- (id) makeSelectedPerform: (SEL)sel;
- (id) makeUnselectedPerform: (SEL)sel;
- (id) makeAllPerform: (SEL)sel;
- (id) makeGlobalPerform: (SEL)sel; // in and out of region

- (id) cloneSelection: sender;

- (id) makeEntity: sender;

- (id) subtractSelection: sender;

- (id) selectCompletelyInside: sender;
- (id) selectPartiallyInside: sender;

- (id) tallBrush: sender;
- (id) shortBrush: sender;

- (id) rotate_x: sender;
- (id) rotate_y: sender;
- (id) rotate_z: sender;

- (id) flip_x: sender;
- (id) flip_y: sender;
- (id) flip_z: sender;

- (id) selectCompleteEntity: sender;

@end

#endif // Map_h
