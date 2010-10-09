#include <unistd.h>

#include "QF/sys.h"
#include "QF/va.h"

#include "Preferences.h"
#include "Map.h"
#include "QuakeEd.h"
#include "Project.h"

id  preferences_i;

#define DEFOWNER "QuakeEd2"

float  lightaxis[3] = {1, 0.6, 0.75};

@implementation Preferences

void
WriteStringDefault (id prefs, NSString *name, NSString *value)
{
	[prefs setObject: value forKey: name];
}

void
WriteNumericDefault (id prefs, NSString *name, float value)
{
	WriteStringDefault (prefs, name, [NSString stringWithFormat: @"%f", value]);
}

- (id) init
{
	[super init];
	preferences_i = self;

	NSMutableDictionary  *defaults = [NSMutableDictionary dictionary];

	WriteStringDefault (defaults, @"ProjectPath", @"");
	WriteStringDefault (defaults, @"BspSoundPath", @"");
	WriteNumericDefault (defaults, @"ShowBSPOutput", NO);
	WriteNumericDefault (defaults, @"OffsetBrushCopy", NO);
	WriteNumericDefault (defaults, @"StartWad", 0);
	WriteNumericDefault (defaults, @"Xlight", 0);
	WriteNumericDefault (defaults, @"Ylight", 0);
	WriteNumericDefault (defaults, @"Zlight", 0);

	prefs = [[NSUserDefaults standardUserDefaults] retain];
	[prefs registerDefaults: defaults];
	return self;
}

//
//  Read in at start of program
//
- (id) readDefaults
{
	[self setProjectPath: [prefs stringForKey: @"ProjectPath"]];
	[self setBspSoundPath: [prefs stringForKey: @"BspSoundPath"]];

	[self setShowBSP: [[prefs stringForKey: @"ShowBSPOutput"] intValue]];
	[self setBrushOffset: [[prefs stringForKey: @"OffsetBrushCopy"] intValue]];
	[self setStartWad: [[prefs stringForKey: @"StartWad"] intValue]];

	[self setXlight: [[prefs stringForKey: @"Xlight"] floatValue]];
	[self setYlight: [[prefs stringForKey: @"Ylight"] floatValue]];
	[self setZlight: [[prefs stringForKey: @"Zlight"] floatValue]];

	return self;
}

- (id) setProjectPath: (NSString *)path
{
	[path retain];
	[projectpath release];
	projectpath = path;
	[startproject_i setStringValue: projectpath];
	WriteStringDefault (prefs, @"ProjectPath", projectpath);
	return self;
}

- (id) setCurrentProject: sender
{
	[startproject_i setStringValue: [project_i currentProjectFile]];
	[self UIChanged: self];
	return self;
}

- (NSString *) getProjectPath
{
	return projectpath;
}

//
// ===============================================
//  BSP sound stuff
// ===============================================
//
//  Set the BSP sound using an OpenPanel
//
- (id) setBspSound: sender
{
	id          panel;
	NSString    *types[] = {@"snd"};
	int         rtn;
	NSArray     *filenames;
	NSString    *path, *file;

	panel = [NSOpenPanel new];

	path = [bspSound stringByDeletingLastPathComponent];
	file = [bspSound lastPathComponent];

	rtn = [panel    runModalForDirectory: path file: file
	                               types: [NSArray arrayWithObjects: types
								                              count: 1]
	      ];

	if (rtn) {
		filenames = [panel filenames];
		[self setBspSoundPath: [filenames objectAtIndex: 0]];
		[self playBspSound];
	}

	return self;
}

//
//  Play the BSP sound
//
- (id) playBspSound
{
	[bspSound_i play];
	return self;
}

//
//  Set the bspSound path
//
- (id) setBspSoundPath: (NSString *)path
{
	
	[path retain];
	[bspSound release];
	bspSound = path;

	if (bspSound_i) {
		[bspSound_i release];
		bspSound_i = nil;
	}
	if (access ([path cString], R_OK)) {
		bspSound_i = [[NSSound alloc] initWithContentsOfFile: bspSound
		                                         byReference: YES];
	}
	if (!bspSound_i)
		return self;

	[bspSoundField_i setStringValue: bspSound];

	WriteStringDefault (prefs, @"BspSoundPath", bspSound);

	return self;
}

// ===============================================
//  Show BSP Output management
// ===============================================

//
//  Set the state
//
- (id) setShowBSP: (int)state
{
	showBSP = state;
	[showBSP_i setIntValue: state];
	WriteNumericDefault (prefs, @"ShowBSPOutput", showBSP);

	return self;
}

//
//  Get the state
//
- (int) getShowBSP
{
	return showBSP;
}

// ===============================================
//  "Offset Brush ..." management
// ===============================================

//
//  Set the state
//
- (id) setBrushOffset: (int)state
{
	brushOffset = state;
	[brushOffset_i setIntValue: state];
	WriteNumericDefault (prefs, @"OffsetBrushCopy", state);
	return self;
}

//
//  Get the state
//
- (int) getBrushOffset
{
	return brushOffset;
}

// ===============================================
//  StartWad
// ===============================================

- (id) setStartWad: (int)value      // set start wad (0-2)
{
	startwad = value;
	if (startwad < 0 || startwad > 2)
		startwad = 0;

	[startwad_i selectCellAtRow: startwad column: 0];

	WriteNumericDefault (prefs, @"StartWad", value);
	return self;
}

- (int) getStartWad
{
	return startwad;
}

// ===============================================
//  X,Y,Z light values
// ===============================================
//
//  Set the state
//
- (id) setXlight: (float)value
{
	xlight = value;
	if (xlight < 0.25 || xlight > 1)
		xlight = 0.6;
	lightaxis[1] = xlight;
	[xlight_i setFloatValue: xlight];
	WriteNumericDefault (prefs, @"Xlight", xlight);
	return self;
}

- (id) setYlight: (float)value
{
	ylight = value;
	if (ylight < 0.25 || ylight > 1)
		ylight = 0.75;
	lightaxis[2] = ylight;
	[ylight_i setFloatValue: ylight];
	WriteNumericDefault (prefs, @"Ylight", ylight);
	return self;
}

- (id) setZlight: (float)value
{
	zlight = value;
	if (zlight < 0.25 || zlight > 1)
		zlight = 1;
	lightaxis[0] = zlight;
	[zlight_i setFloatValue: zlight];
	WriteNumericDefault (prefs, @"Zlight", zlight);
	return self;
}

//
//  Get the state
//
- (float) getXlight
{
	return [xlight_i floatValue];
}

- (float) getYlight
{
	return [ylight_i floatValue];
}

- (float) getZlight
{
	return [zlight_i floatValue];
}

/*
============
UIChanged

Grab all the current UI state
============
*/
- (id) UIChanged: sender
{
	Sys_Printf ("defaults updated\n");

	[self setProjectPath: [startproject_i stringValue]];
	[self setBspSoundPath: [bspSoundField_i stringValue]];
	[self setShowBSP: [showBSP_i intValue]];
	[self setBrushOffset: [brushOffset_i intValue]];
	[self setStartWad: [startwad_i selectedRow]];
	[self setXlight: [xlight_i floatValue]];
	[self setYlight: [ylight_i floatValue]];
	[self setZlight: [zlight_i floatValue]];

	[map_i makeGlobalPerform: @selector (flushTextures)];
	[quakeed_i updateAll];

	return self;
}

@end
