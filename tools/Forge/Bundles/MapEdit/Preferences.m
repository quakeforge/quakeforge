
#include "Preferences.h"
#include "Map.h"
#include "QuakeEd.h"
#include "Project.h"

id          preferences_i;

#define	DEFOWNER	"QuakeEd2"

float       lightaxis[3] = { 1, 0.6, 0.75 };

@implementation Preferences

-init
{
	[super init];
	preferences_i = self;
	return self;
}

int
_atoi (char *c)
{
	if (!c)
		return 0;
	return atoi (c);
}

int
_atof (char *c)
{
	if (!c)
		return 0;
	return atof (c);
}

void
WriteNumericDefault (char *name, float value)
{
	char        str[128];

	sprintf (str, "%f", value);
	// XXX NSWriteDefault (DEFOWNER, name, str);
}

void
WriteStringDefault (char *name, char *value)
{
	// XXX NSWriteDefault (DEFOWNER, name, value);
}

//
//  Read in at start of program
//
-readDefaults
{
	char       *string = "";
	float       value = 0;

	// XXX string = (char *)NSGetDefaultValue(DEFOWNER,"ProjectPath");
	[self setProjectPath:string];

	// XXX string = (char *)NSGetDefaultValue(DEFOWNER,"BspSoundPath");
	[self setBspSoundPath:string];

	// XXX value = _atoi((char *)NSGetDefaultValue(DEFOWNER,"ShowBSPOutput"));
	[self setShowBSP:value];

	// XXX value = _atoi((char
	// *)NSGetDefaultValue(DEFOWNER,"OffsetBrushCopy"));
	[self setBrushOffset:value];

	// XXX value = _atoi((char *)NSGetDefaultValue(DEFOWNER,"StartWad"));
	[self setStartWad:value];

	// XXX value = _atof((char *)NSGetDefaultValue(DEFOWNER,"Xlight"));
	[self setXlight:value];

	// XXX value = _atof((char *)NSGetDefaultValue(DEFOWNER,"Ylight"));
	[self setYlight:value];

	// XXX value = _atof((char *)NSGetDefaultValue(DEFOWNER,"Zlight"));
	[self setZlight:value];

	return self;
}


-setProjectPath:(char *) path
{
	if (!path)
		path = "";
	strcpy (projectpath, path);
	[startproject_i setStringValue: [NSString stringWithCString:path]];
	WriteStringDefault ("ProjectPath", path);
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

	panel =[NSOpenPanel new];

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
-setBspSoundPath:(char *) path
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

	WriteStringDefault ("BspSoundPath", bspSound);

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
	WriteNumericDefault ("ShowBSPOutput", showBSP);

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
	WriteNumericDefault ("OffsetBrushCopy", state);
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

	WriteNumericDefault ("StartWad", value);
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
	WriteNumericDefault ("Xlight", xlight);
	return self;
}

-setYlight:(float) value
{
	ylight = value;
	if (ylight < 0.25 || ylight > 1)
		ylight = 0.75;
	lightaxis[2] = ylight;
	[ylight_i setFloatValue:ylight];
	WriteNumericDefault ("Ylight", ylight);
	return self;
}

-setZlight:(float) value
{
	zlight = value;
	if (zlight < 0.25 || zlight > 1)
		zlight = 1;
	lightaxis[0] = zlight;
	[zlight_i setFloatValue:zlight];
	WriteNumericDefault ("Zlight", zlight);
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
