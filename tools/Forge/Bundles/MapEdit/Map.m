
#include <sys/time.h>
#include <time.h>

#include "QF/quakeio.h"
#include "QF/script.h"
#include "QF/sys.h"

#include "Map.h"
#include "Entity.h"
#include "TexturePalette.h"
#include "SetBrush.h"
#include "XYView.h"
#include "CameraView.h"
#include "QuakeEd.h"
#include "Things.h"
#include "InspectorControl.h"
#include "Project.h"

#define THING Map
#include "THING+NSArray.m"

id          map_i;

@implementation Map
/*
===============================================================================

FILE METHODS

===============================================================================
*/
- init
{
	self = [super init];
	array = [[NSMutableArray alloc] init];
	map_i = self;
	minz = 0;
	maxz = 80;

	oldselection =[[NSMutableArray alloc] init];

	return self;
}

-saveSelected
{
	int         i, c;
	id          o, w;

	[oldselection removeAllObjects];
	w =[self objectAtIndex:0];
	c =[w count];
	sb_newowner = oldselection;
	for (i = 0; i < c; i++) {
		o =[w objectAtIndex:0];
		if ([o selected])
			[o moveToEntity];
		else {
			[w removeObjectAtIndex:0];
		}
	}

	c =[self count];
	for (i = 0; i < c; i++) {
		o =[self objectAtIndex:0];
		[self removeObjectAtIndex:0];
		[o removeAllObjects];
	}

	return self;
}

-addSelected
{
	int         i, c;
	id          n, w;

	c =[oldselection count];
	w =[self objectAtIndex:0];			// world object

	sb_newowner = w;
	for (i = 0; i < c; i++) {
		n =[oldselection objectAtIndex:i];
		[n moveToEntity];
		i--;
		c--;
	}
	[oldselection removeAllObjects];

	return self;
}


-newMap
{
	id          ent;

	[self saveSelected];
	ent =[[Entity alloc] initClass:"worldspawn"];
	[self addObject:ent];
	currentEntity = NULL;
	[self setCurrentEntity:ent];
	[self addSelected];

	return self;
}

-currentEntity
{
	return currentEntity;
}

-setCurrentEntity:ent
{
	id          old;

	old = currentEntity;
	currentEntity = ent;
	if (old != ent) {
		[things_i newCurrentEntity];	// update inspector
		[inspcontrol_i setCurrentInspector: i_things];
	}

	return self;
}

-(float) currentMinZ
{
	float       grid;

	grid =[xyview_i gridsize];
	minz = grid * rint (minz / grid);
	return minz;
}

-setCurrentMinZ:(float) m
{
	if (m > -2048)
		minz = m;
	return self;
}

-(float) currentMaxZ
{
	float       grid;

	[self currentMinZ];					// grid align

	grid =[xyview_i gridsize];
	maxz = grid * rint (maxz / grid);

	if (maxz <= minz)
		maxz = minz + grid;
	return maxz;
}

-setCurrentMaxZ:(float) m
{
	if (m < 2048)
		maxz = m;
	return self;
}

-(void) removeObject:o
{
	[super removeObject:o];

	if (o == currentEntity) {			// select the world
		[self setCurrentEntity: [self objectAtIndex:0]];
	}

	return;
}

#define FN_DEVLOG "/qcache/devlog"
-writeStats
{
	FILE	*f;
	extern	int	c_updateall;
	struct timeval tp;
	struct timezone tzp;

	gettimeofday(&tp, &tzp);
	
	f = fopen (FN_DEVLOG, "a");
	fprintf (f,"%i %i\n", (int)tp.tv_sec, c_updateall);
	c_updateall = 0;
	fclose (f);
	return self;
}

-(int) numSelected
{
	int         i, c;
	int         num;

	num = 0;
	c =[currentEntity count];
	for (i = 0; i < c; i++)
		if ([[currentEntity objectAtIndex:i] selected])
			num++;
	return num;
}

-selectedBrush
{
	int         i, c;
	int         num;

	num = 0;
	c =[currentEntity count];
	for (i = 0; i < c; i++)
		if ([[currentEntity objectAtIndex:i] selected])
			return[currentEntity objectAtIndex:i];
	return nil;
}


/*
=================
readMapFile
=================
*/
-readMapFile:(const char *) fname
{
	char       *dat;
	const char *wad, *cl;
	id          new;
	id          ent;
	int         i, c;
	vec3_t      org;
	float       angle;
	QFile      *file;
	script_t   *script;
	size_t      size;

	[self saveSelected];

	Sys_Printf ("loading %s\n", fname);

	file = Qopen (fname, "rt");
	if (!file)
		return self;
	size = Qfilesize (file);
	dat = malloc (size + 1);
	size = Qread (file, dat, size);
	Qclose (file);
	dat[size] = 0;

	script = Script_New ();
	Script_Start (script, fname, dat);

	do {
		new =[[Entity alloc] initFromScript:script];
		if (!new)
			break;
		[self addObject:new];
	} while (1);

	free (dat);

	[self addSelected];

// load the apropriate texture wad
	wad =[currentEntity valueForQKey:"wad"];
	if (wad && wad[0]) {
		if (wad[0] == '/')				// remove old style fullpaths
			[currentEntity removeKeyPair:"wad"];
		else {
			if (strcmp ([texturepalette_i currentWad], wad))
				[project_i setTextureWad:wad];
		}
	}

	[self setCurrentEntity: [self objectAtIndex:0]];
// center the camera and XY view on the playerstart
	c =[self count];
	for (i = 1; i < c; i++) {
		ent =[self objectAtIndex:i];
		cl =[ent valueForQKey:"classname"];
		if (cl && !strcasecmp (cl, "info_player_start")) {
			angle = atof ([ent valueForQKey:"angle"]);
			angle = angle / 180 * M_PI;
			[ent getVector: org forKey:"origin"];
			[cameraview_i setOrigin: org angle:angle];
			[xyview_i centerOn:org];
			break;
		}
	}

	return self;
}

/*
=================
writeMapFile
=================
*/
-writeMapFile:(const char *)
fname       useRegion:(BOOL) reg
{
	FILE       *f;
	unsigned int i;

	Sys_Printf ("writeMapFile: %s\n", fname);

	f = fopen (fname, "w");
	if (!f)
		Sys_Error ("couldn't write %s", fname);

	for (i = 0; i <[self count]; i++)
		[[self objectAtIndex: i] writeToFILE: f region:reg];

	fclose (f);

	return self;
}

/*
==============================================================================

DRAWING

==============================================================================
*/

-(void)ZDrawSelf
{
	int         i, count;

	count =[self count];

	for (i = 0; i < count; i++)
		[[self objectAtIndex:i] ZDrawSelf];
}

-(void)RenderSelf:(void (*)(face_t *)) callback
{
	int         i, count;

	count =[self count];

	for (i = 0; i < count; i++)
		[[self objectAtIndex: i] RenderSelf:callback];
}


//============================================================================


/*
===================
entityConnect

A command-shift-click on an entity while an entity is selected will
make a target connection from the original entity.
===================
*/
-entityConnect: (vec3_t) p1:(vec3_t) p2
{
	id          oldent, ent;

	oldent =[self currentEntity];
	if (oldent ==[self objectAtIndex:0]) {
		Sys_Printf ("Must have a non-world entity selected to connect\n");
		return self;
	}

	[self selectRay: p1: p2:YES];
	ent =[self currentEntity];
	if (ent == oldent) {
		Sys_Printf ("Must click on a different entity to connect\n");
		return self;
	}

	if (ent ==[self objectAtIndex:0]) {
		Sys_Printf ("Must click on a non-world entity to connect\n");
		return self;
	}

	[oldent setKey: "target" toValue:[ent targetname]];
	[quakeed_i updateAll];

	return self;
}


/*
=================
selectRay

If ef is true, any entity brush along the ray will be selected in preference
to intervening world brushes
=================
*/
-selectRay: (vec3_t) p1: (vec3_t) p2:(BOOL) ef
{
	int         i, j, c, c2;
	id          ent, bestent;
	id          brush, bestbrush;
	int         face, bestface;
	float       time, besttime;
	texturedef_t *td;

	bestent = nil;
	bestface = -1;
	bestbrush = nil;
	besttime = 99999;

	c =[self count];
	for (i = c - 1; i >= 0; i--) {
		ent =[self objectAtIndex:i];
		c2 =[ent count];
		for (j = 0; j < c2; j++) {
			brush =[ent objectAtIndex:j];
			[brush hitByRay: p1: p2: &time:&face];
			if (time < 0 || time > besttime)
				continue;
			bestent = ent;
			besttime = time;
			bestbrush = brush;
			bestface = face;
		}
		if (i == 1 && ef && bestbrush)
			break;						// found an entity, so don't check the
										// world
	}

	if (besttime == 99999) {
		Sys_Printf ("trace missed\n");
		return self;
	}

	if ([bestbrush regioned]) {
		Sys_Printf ("WANRING: clicked on regioned brush\n");
		return self;
	}

	if (bestent != currentEntity) {
		[self makeSelectedPerform:@selector (deselect)];
		[self setCurrentEntity:bestent];
	}

	[quakeed_i disableFlushWindow];
	if (![bestbrush selected]) {
		if ([map_i numSelected] == 0) {	// don't grab texture if others are
										// selected
			td =[bestbrush texturedefForFace:bestface];
			[texturepalette_i setTextureDef:td];
		}

		[bestbrush setSelected:YES];
		Sys_Printf ("selected entity %i brush %i face %i\n",
				    (int)[self indexOfObject: bestent],
					(int)[bestent indexOfObject:bestbrush], bestface);
	} else {
		[bestbrush setSelected:NO];
		Sys_Printf ("deselected entity %i brush %i face %i\n",
					(int)[self indexOfObject: bestent],
					(int)[bestent indexOfObject:bestbrush], bestface);
	}

	[quakeed_i enableFlushWindow];
	[quakeed_i updateAll];

	return self;
}

/*
=================
grabRay

checks only the selected brushes
Returns the brush hit, or nil if missed.
=================
*/
-grabRay: (vec3_t) p1:(vec3_t) p2
{
	int         i, j, c, c2;
	id          ent;
	id          brush, bestbrush;
	int         face;
	float       time, besttime;

	bestbrush = nil;
	besttime = 99999;

	c =[self count];
	for (i = 0; i < c; i++) {
		ent =[self objectAtIndex:i];
		c2 =[ent count];
		for (j = 0; j < c2; j++) {
			brush =[ent objectAtIndex:j];
			if (![brush selected])
				continue;
			[brush hitByRay: p1: p2: &time:&face];
			if (time < 0 || time > besttime)
				continue;
			besttime = time;
			bestbrush = brush;
		}
	}

	if (besttime == 99999)
		return nil;

	return bestbrush;
}

/*
=================
getTextureRay
=================
*/
-getTextureRay: (vec3_t) p1:(vec3_t) p2
{
	int         i, j, c, c2;
	id          ent, bestent;
	id          brush, bestbrush;
	int         face, bestface;
	float       time, besttime;
	texturedef_t *td;
	vec3_t      mins, maxs;


	bestbrush = nil;
	bestent = nil;
	besttime = 99999;
	bestface = -1;
	c =[self count];
	for (i = 0; i < c; i++) {
		ent =[self objectAtIndex:i];
		c2 =[ent count];
		for (j = 0; j < c2; j++) {
			brush =[ent objectAtIndex:j];
			[brush hitByRay: p1: p2: &time:&face];
			if (time < 0 || time > besttime)
				continue;
			bestent = ent;
			bestface = face;
			besttime = time;
			bestbrush = brush;
		}
	}

	if (besttime == 99999)
		return nil;

	if (![bestent modifiable]) {
		Sys_Printf ("can't modify spawned entities\n");
		return self;
	}

	td =[bestbrush texturedefForFace:bestface];
	[texturepalette_i setTextureDef:td];

	Sys_Printf ("grabbed texturedef and sizes\n");

	[bestbrush getMins: mins maxs:maxs];

	minz = mins[2];
	maxz = maxs[2];

	return bestbrush;
}

/*
=================
setTextureRay
=================
*/
-setTextureRay: (vec3_t) p1: (vec3_t) p2:(BOOL) allsides;
{
	int         i, j, c, c2;
	id          ent, bestent;
	id          brush, bestbrush;
	int         face, bestface;
	float       time, besttime;
	texturedef_t td;

	bestent = nil;
	bestface = -1;
	bestbrush = nil;
	besttime = 99999;

	c =[self count];
	for (i = 0; i < c; i++) {
		ent =[self objectAtIndex:i];
		c2 =[ent count];
		for (j = 0; j < c2; j++) {
			brush =[ent objectAtIndex:j];
			[brush hitByRay: p1: p2: &time:&face];
			if (time < 0 || time > besttime)
				continue;
			bestent = ent;
			besttime = time;
			bestbrush = brush;
			bestface = face;
		}
	}

	if (besttime == 99999) {
		Sys_Printf ("trace missed\n");
		return self;
	}

	if (![bestent modifiable]) {
		Sys_Printf ("can't modify spawned entities\n");
		return self;
	}

	if ([bestbrush regioned]) {
		Sys_Printf ("WANRING: clicked on regioned brush\n");
		return self;
	}

	[texturepalette_i getTextureDef:&td];

	[quakeed_i disableFlushWindow];
	if (allsides) {
		[bestbrush setTexturedef:&td];
		Sys_Printf ("textured entity %i brush %i\n", (int)[self indexOfObject: bestent], (int)[bestent indexOfObject:bestbrush]);
	} else {
		[bestbrush setTexturedef: &td forFace:bestface];
		Sys_Printf ("deselected entity %i brush %i face %i\n", (int)[self indexOfObject: bestent], (int)[bestent indexOfObject:bestbrush], bestface);
	}
	[quakeed_i enableFlushWindow];

	[quakeed_i updateAll];

	return self;
}


/*
==============================================================================

OPERATIONS ON SELECTIONS

==============================================================================
*/

-makeSelectedPerform:(SEL) sel
{
	int         i, j, c, c2;
	id          ent, brush;
	int         total;

	total = 0;
	c =[self count];
	for (i = c - 1; i >= 0; i--) {
		ent =[self objectAtIndex:i];
		c2 =[ent count];
		for (j = c2 - 1; j >= 0; j--) {
			brush =[ent objectAtIndex:j];
			if (![brush selected])
				continue;
			if ([brush regioned])
				continue;
			total++;
			[brush performSelector:sel];
		}
	}

//  if (!total)
//      Sys_Printf ("nothing selected\n");

	return self;
}

-makeUnselectedPerform:(SEL) sel
{
	int         i, j, c, c2;
	id          ent, brush;

	c =[self count];
	for (i = c - 1; i >= 0; i--) {
		ent =[self objectAtIndex:i];
		c2 =[ent count];
		for (j = c2 - 1; j >= 0; j--) {
			brush =[ent objectAtIndex:j];
			if ([brush selected])
				continue;
			if ([brush regioned])
				continue;
			[brush performSelector:sel];
		}
	}

	return self;
}

-makeAllPerform:(SEL) sel
{
	int         i, j, c, c2;
	id          ent, brush;

	c =[self count];
	for (i = c - 1; i >= 0; i--) {
		ent =[self objectAtIndex:i];
		c2 =[ent count];
		for (j = c2 - 1; j >= 0; j--) {
			brush =[ent objectAtIndex:j];
			if ([brush regioned])
				continue;
			[brush performSelector:sel];
		}
	}

	return self;
}

-makeGlobalPerform:(SEL) sel			// in and out of region
{
	int         i, j, c, c2;
	id          ent, brush;

	c =[self count];
	for (i = c - 1; i >= 0; i--) {
		ent =[self objectAtIndex:i];
		c2 =[ent count];
		for (j = c2 - 1; j >= 0; j--) {
			brush =[ent objectAtIndex:j];
			[brush performSelector:sel];
		}
	}

	return self;
}


void
sel_identity (void)
{
	sel_x[0] = 1;
	sel_x[1] = 0;
	sel_x[2] = 0;
	sel_y[0] = 0;
	sel_y[1] = 1;
	sel_y[2] = 0;
	sel_z[0] = 0;
	sel_z[1] = 0;
	sel_z[2] = 1;
}

-transformSelection
{
	if (![currentEntity modifiable]) {
		Sys_Printf ("can't modify spawned entities\n");
		return self;
	}
// find an origin to apply the transformation to
	sb_mins[0] = sb_mins[1] = sb_mins[2] = 99999;
	sb_maxs[0] = sb_maxs[1] = sb_maxs[2] = -99999;
	[self makeSelectedPerform:@selector (addToBBox)];
	sel_org[0] =[xyview_i snapToGrid:(sb_mins[0] + sb_maxs[0]) / 2];
	sel_org[1] =[xyview_i snapToGrid:(sb_mins[1] + sb_maxs[1]) / 2];
	sel_org[2] =[xyview_i snapToGrid:(sb_mins[2] + sb_maxs[2]) / 2];

// do it!
	[self makeSelectedPerform:@selector (transform)];

	[quakeed_i updateAll];
	return self;
}


void
swapvectors (vec3_t a, vec3_t b)
{
	vec3_t      temp;

	VectorCopy (a, temp);
	VectorCopy (b, a);
	VectorSubtract (vec3_origin, temp, b);
}

/*
===============================================================================

UI operations

===============================================================================
*/

-rotate_x:sender
{
	sel_identity ();
	swapvectors (sel_y, sel_z);
	[self transformSelection];
	return self;
}

-rotate_y:sender
{
	sel_identity ();
	swapvectors (sel_x, sel_z);
	[self transformSelection];
	return self;
}

-rotate_z:sender
{
	sel_identity ();
	swapvectors (sel_x, sel_y);
	[self transformSelection];
	return self;
}


-flip_x:sender
{
	sel_identity ();
	sel_x[0] = -1;
	[self transformSelection];
	[map_i makeSelectedPerform:@selector (flipNormals)];
	return self;
}

-flip_y:sender
{
	sel_identity ();
	sel_y[1] = -1;
	[self transformSelection];
	[map_i makeSelectedPerform:@selector (flipNormals)];
	return self;
}


-flip_z:sender
{
	sel_identity ();
	sel_z[2] = -1;
	[self transformSelection];
	[map_i makeSelectedPerform:@selector (flipNormals)];
	return self;
}


-cloneSelection:sender
{
	int         i, j, c, originalElements;
	id          o, b;
	id          new;

	sb_translate[0] = sb_translate[1] =[xyview_i gridsize];
	sb_translate[2] = 0;

// copy individual brushes in the world entity
	o =[self objectAtIndex:0];
	c =[o count];
	for (i = 0; i < c; i++) {
		b =[o objectAtIndex:i];
		if (![b selected])
			continue;

		// copy the brush, then translate the original
		new =[b copy];
		[new setSelected:YES];
		[new translate];
		[b setSelected:NO];
		[o addObject:new];
	}

// copy entire entities otherwise
	originalElements =[self count];		// don't copy the new ones
	for (i = 1; i < originalElements; i++) {
		o =[self objectAtIndex:i];
		if (![[o objectAtIndex:0] selected])
			continue;

		new =[o copy];
		[self addObject:new];

		c =[o count];
		for (j = 0; j < c; j++)
			[[o objectAtIndex: j] setSelected:NO];

		c =[new count];
		for (j = 0; j < c; j++) {
			b =[new objectAtIndex:j];
			[b translate];
			[b setSelected:YES];
		}
	}

	[quakeed_i updateAll];

	return self;
}


-selectCompleteEntity:sender
{
	id          o;
	int         i, c;

	o =[self selectedBrush];
	if (!o) {
		Sys_Printf ("nothing selected\n");
		return self;
	}
	o =[o parent];
	c =[o count];
	for (i = 0; i < c; i++)
		[[o objectAtIndex: i] setSelected:YES];
	Sys_Printf ("%i brushes selected\n", c);

	[quakeed_i updateAll];

	return self;
}

-makeEntity:sender
{
	if (currentEntity !=[self objectAtIndex:0]) {
		Sys_Printf ("ERROR: can't makeEntity inside an entity\n");
		NSBeep ();
		return self;
	}

	if ([self numSelected] == 0) {
		Sys_Printf ("ERROR: must have a seed brush to make an entity\n");
		NSBeep ();
		return self;
	}

	sb_newowner =[[Entity alloc] initClass:[things_i spawnName]];

	if ([sb_newowner modifiable])
		[self makeSelectedPerform:@selector (moveToEntity)];
	else {								// throw out seed brush and select
										// entity fixed brush
		[self makeSelectedPerform:@selector (remove)];
		[[sb_newowner objectAtIndex: 0] setSelected:YES];
	}

	[self addObject:sb_newowner];
	[self setCurrentEntity:sb_newowner];

	[quakeed_i updateAll];

	return self;
}


-selbox:(SEL) selector
{
	id          b;

	if ([self numSelected] != 1) {
		Sys_Printf ("must have a single brush selected\n");
		return self;
	}

	b =[self selectedBrush];
	[b getMins: select_min maxs:select_max];
	[b remove];

	[self makeUnselectedPerform:selector];

	Sys_Printf ("identified contents\n");
	[quakeed_i updateAll];

	return self;
}

-selectCompletelyInside:sender
{
	return[self selbox:@selector (selectComplete)];
}

-selectPartiallyInside:sender
{
	return[self selbox:@selector (selectPartial)];
}


-tallBrush:sender
{
	id          b;
	vec3_t      mins, maxs;
	texturedef_t td;

	if ([self numSelected] != 1) {
		Sys_Printf ("must have a single brush selected\n");
		return self;
	}

	b =[self selectedBrush];
	td = *[b texturedef];
	[b getMins: mins maxs:maxs];
	[b remove];

	mins[2] = -2048;
	maxs[2] = 2048;

	b =[[SetBrush alloc] initOwner: [map_i objectAtIndex: 0] mins: mins maxs: maxs texture:&td];
	[[map_i objectAtIndex: 0] addObject:b];
	[b setSelected:YES];
	[quakeed_i updateAll];

	return self;
}

-shortBrush:sender
{
	id          b;
	vec3_t      mins, maxs;
	texturedef_t td;

	if ([self numSelected] != 1) {
		Sys_Printf ("must have a single brush selected\n");
		return self;
	}

	b =[self selectedBrush];
	td = *[b texturedef];
	[b getMins: mins maxs:maxs];
	[b remove];

	mins[2] = 0;
	maxs[2] = 16;

	b =[[SetBrush alloc] initOwner: [map_i objectAtIndex: 0] mins: mins maxs: maxs texture:&td];
	[[map_i objectAtIndex: 0] addObject:b];
	[b setSelected:YES];
	[quakeed_i updateAll];

	return self;
}

/*
==================
subtractSelection
==================
*/
-subtractSelection:semder
{
	int         i, j, c, c2;
	id          o, o2;
	id          sellist, sourcelist;

	Sys_Printf ("performing brush subtraction...\n");

	sourcelist =[[NSMutableArray alloc] init];
	sellist =[[NSMutableArray alloc] init];
	carve_in =[[NSMutableArray alloc] init];
	carve_out =[[NSMutableArray alloc] init];

	c =[currentEntity count];
	for (i = 0; i < c; i++) {
		o =[currentEntity objectAtIndex:i];
		if ([o selected])
			[sellist addObject:o];
		else
			[sourcelist addObject:o];
	}


	c =[sellist count];
	for (i = 0; i < c; i++) {
		o =[sellist objectAtIndex:i];
		[o setCarveVars];

		c2 =[sourcelist count];
		for (j = 0; j < c2; j++) {
			o2 =[sourcelist objectAtIndex:j];
			[o2 carve];
			[carve_in removeAllObjects];
		}

		[sourcelist release];			// the individual have been moved/freed
		sourcelist = carve_out;
		carve_out =[[NSMutableArray alloc] init];
	}

// add the selection back to the remnants
	[currentEntity removeAllObjects];
	[currentEntity addObjectsFromArray:sourcelist];
	[currentEntity addObjectsFromArray:sellist];

	[sourcelist release];
	[sellist release];
	[carve_in release];
	[carve_out release];

	if (![currentEntity count]) {
		o = currentEntity;
		[self removeObject:o];
	}

	Sys_Printf ("subtracted selection\n");
	[quakeed_i updateAll];

	return self;
}


@end
