
// Map is a collection of Entity objects

extern id	map_i;

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
