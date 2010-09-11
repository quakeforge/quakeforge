
#include "Preferences.h"
#include "Map.h"
#include "QuakeEd.h"
#include "Project.h"

id          preferences_i;

#define	DEFOWNER	"QuakeEd2"

float       lightaxis[3] = { 1, 0.6, 0.75 };

@implementation Preferences

void
WriteStringDefault (id prefs, const char *name, const char *value)
{
	NSString   *key = [NSString stringWithCString: name];
	NSString   *val = [NSString stringWithCString: value];
	[prefs setObject:val forKey:key];
}

void
WriteNumericDefault (id prefs, const char *name, float value)
{
	char        str[128];

	sprintf (str, "%f", value);
	WriteStringDefault (prefs, name, str);
}

-init
{
	[super init];
	preferences_i = self;

	NSMutableDictionary *defaults = [NSMutableDictionary dictionary];

	WriteStringDefault (defaults, "ProjectPath", "");
	WriteStringDefault (defaults, "BspSoundPath", "");
	WriteNumericDefault (defaults, "ShowBSPOutput", NO);
	WriteNumericDefault (defaults, "OffsetBrushCopy", NO);
	WriteNumericDefault (defaults, "StartWad", 0);
	WriteNumericDefault (defaults, "Xlight", 0);
	WriteNumericDefault (defaults, "Ylight", 0);
	WriteNumericDefault (defaults, "Zlight", 0);

	prefs = [[NSUserDefaults standardUserDefaults] retain];
	[prefs registerDefaults:defaults];
	return self;
}

int
_atoi (const char *c)
{
	if (!c)
		return 0;
	return atoi (c);
}

int
_atof (const char *c)
{
	if (!c)
		return 0;
	return atof (c);
}

//
//  Read in at start of program
//
-readDefaults
{
	const char *string;
	float       value = 0;

	string = [[prefs stringForKey: @"ProjectPath"] cString];
	[self setProjectPath:string];

	string = [[prefs stringForKey: @"BspSoundPath"] cString];
	[self setBspSoundPath:string];

	string = [[prefs stringForKey: @"ShowBSPOutput"] cString];
	value = _atoi (string);
	[self setShowBSP:value];

	string = [[prefs stringForKey: @"OffsetBrushCopy"] cString];
	value = _atoi (string);
	[self setBrushOffset:value];

	string = [[prefs stringForKey: @"StartWad"] cString];
	value = _atoi (string);
	[self setStartWad:value];

	string = [[prefs stringForKey: @"Xlight"] cString];
	value = _atof (string);
	[self setXlight:value];

	string = [[prefs stringForKey: @"Ylight"] cString];
	value = _atof (string);
	[self setYlight:value];

	string = [[prefs stringForKey: @"Zlight"] cString];
	value = _atof (string);
	[self setZlight:value];

	return self;
}


-setProjectPath:(const char *) path
{
	if (!path)
		path = "";
	strcpy (projectpath, path);
	[startproject_i setStringValue: [NSString stringWithCString:path]];
	WriteStringDefault (prefs, "ProjectPath", path);
	return self;
}

-setCurrentProject:sender
{
	[startproject_i setStringValue: [NSString stringWithCString:[project_i
	 currentProjectFile]]];
	[self UIChanged:self];
	return self;
}

-(char *) getProjectPath
{
	return projectpath;
}


//
//===============================================
//  BSP sound stuff
//===============================================
//
//  Set the BSP sound using an OpenPanel
//
-setBspSound:sender
{
	id          panel;
	NSString   *types[] = { @"snd" };
	int         rtn;
	NSArray    *filenames;
	char        path[1024], file[64];

	panel = [NSOpenPanel new];

	// XXX ExtractFilePath (bspSound, path);
	// XXX ExtractFileBase (bspSound, file);

	rtn =[panel runModalForDirectory: [NSString stringWithCString:path]
	file: [NSString stringWithCString:file]
	types: [NSArray arrayWithObjects: types count:1]];

	if (rtn) {
		filenames =[panel filenames];
		strcpy (bspSound,[[panel directory] cString]);
		strcat (bspSound, "/");
		strcat (bspSound,[[filenames objectAtIndex:0] cString]);
		[self setBspSoundPath:bspSound];
		[self playBspSound];
	}

	return self;
}


//
//  Play the BSP sound
//
-playBspSound
{
	[bspSound_i play];
	return self;
}


//
//  Set the bspSound path
//
-setBspSoundPath:(const char *) path
{
	if (!path)
		path = "";
	strcpy (bspSound, path);

	if (bspSound_i)
		[bspSound_i release];
	bspSound_i =[[NSSound alloc] initWithContentsOfFile: [NSString stringWithCString:bspSound]];
	if (!bspSound_i) {
		strcpy (bspSound, "/NextLibrary/Sounds/Funk.snd");
		bspSound_i =[[NSSound alloc] initWithContentsOfFile: [NSString stringWithCString:bspSound]];
	}

	[bspSoundField_i setStringValue: [NSString stringWithCString:bspSound]];

	WriteStringDefault (prefs, "BspSoundPath", bspSound);

	return self;
}

//===============================================
//  Show BSP Output management
//===============================================

//
//  Set the state
//
-setShowBSP:(int) state
{
	showBSP = state;
	[showBSP_i setIntValue:state];
	WriteNumericDefault (prefs, "ShowBSPOutput", showBSP);

	return self;
}

//
//  Get the state
//
-(int) getShowBSP
{
	return showBSP;
}


//===============================================
//  "Offset Brush ..." management
//===============================================

//
//  Set the state
//
-setBrushOffset:(int) state
{
	brushOffset = state;
	[brushOffset_i setIntValue:state];
	WriteNumericDefault (prefs, "OffsetBrushCopy", state);
	return self;
}

//
//  Get the state
//
-(int) getBrushOffset
{
	return brushOffset;
}

//===============================================
//  StartWad
//===============================================

-setStartWad:(int) value				// set start wad (0-2)
{
	startwad = value;
	if (startwad < 0 || startwad > 2)
		startwad = 0;

	[startwad_i selectCellAtRow: startwad column:0];

	WriteNumericDefault (prefs, "StartWad", value);
	return self;
}

-(int) getStartWad
{
	return startwad;
}


//===============================================
//  X,Y,Z light values
//===============================================
//
//  Set the state
//
-setXlight:(float) value
{
	xlight = value;
	if (xlight < 0.25 || xlight > 1)
		xlight = 0.6;
	lightaxis[1] = xlight;
	[xlight_i setFloatValue:xlight];
	WriteNumericDefault (prefs, "Xlight", xlight);
	return self;
}

-setYlight:(float) value
{
	ylight = value;
	if (ylight < 0.25 || ylight > 1)
		ylight = 0.75;
	lightaxis[2] = ylight;
	[ylight_i setFloatValue:ylight];
	WriteNumericDefault (prefs, "Ylight", ylight);
	return self;
}

-setZlight:(float) value
{
	zlight = value;
	if (zlight < 0.25 || zlight > 1)
		zlight = 1;
	lightaxis[0] = zlight;
	[zlight_i setFloatValue:zlight];
	WriteNumericDefault (prefs, "Zlight", zlight);
	return self;
}

//
//  Get the state
//
-(float) getXlight
{
	return[xlight_i floatValue];
}

-(float) getYlight
{
	return[ylight_i floatValue];
}

-(float) getZlight
{
	return[zlight_i floatValue];
}



/*
============
UIChanged

Grab all the current UI state
============
*/
-UIChanged:sender
{
	qprintf ("defaults updated");

	[self setProjectPath:(char *)[startproject_i stringValue]];
	[self setBspSoundPath:(char *)[bspSoundField_i stringValue]];
	[self setShowBSP:[showBSP_i intValue]];
	[self setBrushOffset:[brushOffset_i intValue]];
	[self setStartWad:[startwad_i selectedRow]];
	[self setXlight:[xlight_i floatValue]];
	[self setYlight:[ylight_i floatValue]];
	[self setZlight:[zlight_i floatValue]];

	[map_i makeGlobalPerform:@selector (flushTextures)];
	[quakeed_i updateAll];

	return self;
}


@end
