//======================================
//
// QuakeEd Project Management
//
//======================================

#include <unistd.h>

#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "Project.h"
#include "Map.h"
#include "QuakeEd.h"
#include "Preferences.h"
#include "Dict.h"
#include "Things.h"
#include "TexturePalette.h"


id          project_i;

@implementation Project

-init
{
	project_i = self;

	return self;
}

//===========================================================
//
//  Project code
//
//===========================================================
-initVars
{
	const char *s;
	const char *pe;
	char       *ts;

	ts = strdup ([preferences_i getProjectPath]);
	pe = QFS_SkipPath (ts);
	ts[pe - ts] = 0;
	strcpy (path_basepath, ts);

	strcpy (path_progdir, ts);
	strcat (path_progdir, SUBDIR_ENT);

	strcpy (path_mapdirectory, ts);
	strcat (path_mapdirectory, SUBDIR_MAPS);	// source dir

	strcpy (path_finalmapdir, ts);
	strcat (path_finalmapdir, SUBDIR_MAPS);	// dest dir

	[basepathinfo_i setStringValue: [NSString stringWithCString:ts]];
										// in Project Inspector

#if 0
	if ((s =[projectInfo getStringFor:BASEPATHKEY])) {
		strcpy (path_basepath, s);

		strcpy (path_progdir, s);
		strcat (path_progdir, "/" SUBDIR_ENT);

		strcpy (path_mapdirectory, s);
		strcat (path_mapdirectory, "/" SUBDIR_MAPS);	// source dir

		strcpy (path_finalmapdir, s);
		strcat (path_finalmapdir, "/" SUBDIR_MAPS);	// dest dir

		[basepathinfo_i setStringValue:s];
										// in Project Inspector
	}
#endif

	if ((s =[projectInfo getStringFor:BSPFULLVIS])) {
		strcpy (string_fullvis, s);
		changeString ('@', '\"', string_fullvis);
	}

	if ((s =[projectInfo getStringFor:BSPFASTVIS])) {
		strcpy (string_fastvis, s);
		changeString ('@', '\"', string_fastvis);
	}

	if ((s =[projectInfo getStringFor:BSPNOVIS])) {
		strcpy (string_novis, s);
		changeString ('@', '\"', string_novis);
	}

	if ((s =[projectInfo getStringFor:BSPRELIGHT])) {
		strcpy (string_relight, s);
		changeString ('@', '\"', string_relight);
	}

	if ((s =[projectInfo getStringFor:BSPLEAKTEST])) {
		strcpy (string_leaktest, s);
		changeString ('@', '\"', string_leaktest);
	}

	if ((s =[projectInfo getStringFor:BSPENTITIES])) {
		strcpy (string_entities, s);
		changeString ('@', '\"', string_entities);
	}
	// Build list of wads 
	wadList =[projectInfo parseMultipleFrom:WADSKEY];

	// Build list of maps & descriptions
	mapList =[projectInfo parseMultipleFrom:MAPNAMESKEY];
	descList =[projectInfo parseMultipleFrom:DESCKEY];
	[self changeChar: '_' to: ' ' in:descList];

	[self initProjSettings];

	return self;
}

//
//  Init Project Settings fields
//
-initProjSettings
{
	[pis_basepath_i setStringValue: [NSString stringWithCString:path_basepath]];
	[pis_fullvis_i setStringValue: [NSString stringWithCString:string_fullvis]];
	[pis_fastvis_i setStringValue: [NSString stringWithCString:string_fastvis]];
	[pis_novis_i setStringValue: [NSString stringWithCString:string_novis]];
	[pis_relight_i setStringValue: [NSString stringWithCString:string_relight]];
	[pis_leaktest_i setStringValue: [NSString stringWithCString:string_leaktest]];

	return self;
}

//
//  Add text to the BSP Output window
//
-addToOutput:(const char *) string
{
	int         end;

	end =[BSPoutput_i textLength];
	[BSPoutput_i replaceCharactersInRange: NSMakeRange (end, 0) withString: [NSString stringWithCString:string]];

	end =[BSPoutput_i textLength];
	[BSPoutput_i setSelectedRange:NSMakeRange (end, 0)];
	// XXX [BSPoutput_i scrollSelToVisible];

	return self;
}

-clearBspOutput:sender
{
	int         end;

	end =[BSPoutput_i textLength];
	[BSPoutput_i replaceCharactersInRange: NSMakeRange (0, end) withString:@""];

	return self;
}

-print
{
	// XXX [BSPoutput_i printPSCode:self];
	return self;
}


-initProject
{
	[self parseProjectFile];
	if (projectInfo == NULL)
		return self;
	[self initVars];
	[mapbrowse_i setReusesColumns:YES];
	[mapbrowse_i loadColumnZero];
	[pis_wads_i setReusesColumns:YES];
	[pis_wads_i loadColumnZero];

	[things_i initEntities];

	return self;
}

//
//  Change a character to another in a Storage list of strings
//
-changeChar:(char) f to:(char) t in:(id) obj
{
	int         i;
	int         max;
	char       *string;

	max =[obj count];
	for (i = 0; i < max; i++) {
		string =[obj elementAt:i];
		changeString (f, t, string);
	}
	return self;
}

//
//  Fill the QuakeEd Maps or wads browser
//  (Delegate method - delegated in Interface Builder)
//
-(void) browser: sender createRowsForColumn:(int) column inMatrix: matrix
{
	id          cell, list;
	int         max;
	char       *name;
	int         i;

	if (sender == mapbrowse_i)
		list = mapList;
	else if (sender == pis_wads_i)
		list = wadList;
	else {
		list = nil;
		Sys_Error ("Project: unknown browser to fill");
	}

	max =[list count];
	for (i = 0; i < max; i++) {
		name =[list elementAt:i];
		[matrix addRow];
		cell =[matrix cellAtRow: i column:0];
		[cell setStringValue: [NSString stringWithCString:name]];
		[cell setLeaf:YES];
		[cell setLoaded:YES];
	}
}

//
//  Clicked on a map name or description!
//
-clickedOnMap:sender
{
	id          matrix;
	int         row;
	const char *fname;
	id          panel;
	NSModalSession session;

	matrix =[sender matrixInColumn:0];
	row =[matrix selectedRow];
	fname = va ("%s/%s.map", path_mapdirectory,
				(const char *) [mapList elementAt:row]); //XXX Storage

	panel = NSGetAlertPanel (@"Loading...",
							 @"Loading map. Please wait.", NULL, NULL, NULL);

	session = [NSApp beginModalSessionForWindow: panel];
	[NSApp runModalSession: session];

	[quakeed_i doOpen:fname];

	[NSApp endModalSession: session];
	[panel close];
	NSReleaseAlertPanel (panel);
	return self;
}


-setTextureWad:(const char *) wf
{
	int         i, c;
	const char *name;

	Sys_Printf ("loading %s\n", wf);

// set the row in the settings inspector wad browser
	c =[wadList count];
	for (i = 0; i < c; i++) {
		name = (const char *)[wadList elementAt:i]; // XXX Storage
		if (!strcmp (name, wf)) {
			[[pis_wads_i matrixInColumn: 0] selectCellAtRow: i column:0];
			break;
		}
	}

// update the texture inspector
	[texturepalette_i initPaletteFromWadfile:wf];
	[[map_i objectAtIndex: 0] setKey: "wad" toValue:wf];
//  [inspcontrol_i changeInspectorTo:i_textures];

	[quakeed_i updateAll];

	return self;
}

//
//  Clicked on a wad name
//
-clickedOnWad:sender
{
	id          matrix;
	int         row;
	char       *name;

	matrix =[sender matrixInColumn:0];
	row =[matrix selectedRow];

	name = (char *)[wadList elementAt:row]; // XXX Storage
	[self setTextureWad:name];

	return self;
}


//
//  Read in the <name>.QE_Project file
//
-parseProjectFile
{
	const char *path;
	int         rtn;

	path =[preferences_i getProjectPath];
	if (!path || !path[0] || access (path, 0)) {
		rtn = NSRunAlertPanel (@"Project Error!",
								 @"A default project has not been found.\n",
								 @"Open Project", NULL, NULL);
		if ([self openProject] == nil)
			while (1)					// can't run without a project
				[NSApp terminate:self];
		return self;
	}

	[self openProjectFile:path];
	return self;
}

//
//  Loads and parses a project file
//
-openProjectFile:(const char *) path
{
	FILE       *fp;
	struct stat s;
Sys_Printf ("openProjectFile: %s\n", path);
	strcpy (path_projectinfo, path);

	projectInfo = NULL;
	fp = fopen (path, "r+t");
	if (fp == NULL)
		return self;

	stat (path, &s);
	lastModified = s.st_mtime;

	projectInfo =[(Dict *)[Dict alloc] initFromFile:fp];
	fclose (fp);

	return self;
}

-(const char *) currentProjectFile
{
	return path_projectinfo;
}

//
//  Open a project file
//
-openProject
{
	const char *path;
	id          openpanel;
	int         rtn;
	NSString   *projtypes[] = { @"qpr" };
	NSArray    *filenames;
	const char *dir;

	openpanel =[NSOpenPanel new];
	// [openpanel allowMultipleFiles:NO];
	// [openpanel chooseDirectories:NO];
	rtn =[openpanel runModalForTypes: [NSArray arrayWithObjects: projtypes count:1]];
	if (rtn == NSOKButton) {
		filenames =[openpanel filenames];
		dir =[[openpanel directory] cString];
		dir = "";
		path = va ("%s/%s", dir,[[filenames objectAtIndex:0] cString]);
		strcpy (path_projectinfo, path);
		[self openProjectFile:path];
		return self;
	}

	return nil;
}


//
//  Search for a string in a List of strings
//
-(int) searchForString:(const char *) str in:(id) obj
{
	int         i;
	int         max;
	const char *s;

	max =[obj count];
	for (i = 0; i < max; i++) {
		s = (const char *)[obj elementAt:i];	// XXX Storage?
		if (!strcmp (s, str))
			return 1;
	}
	return 0;
}

-(const char *) getMapDirectory
{
	return path_mapdirectory;
}

-(const char *) getFinalMapDirectory
{
	return path_finalmapdir;
}

-(const char *) getProgDirectory
{
	return path_progdir;
}


//
//  Return the WAD name for cmd-8
//
-(const char *) getWAD8
{
	if (!path_wad8[0])
		return NULL;
	return path_wad8;
}

//
//  Return the WAD name for cmd-9
//
-(const char *) getWAD9
{
	if (!path_wad9[0])
		return NULL;
	return path_wad9;
}

//
//  Return the WAD name for cmd-0
//
-(const char *) getWAD0
{
	if (!path_wad0[0])
		return NULL;
	return path_wad0;
}

//
//  Return the FULLVIS cmd string
//
-(const char *) getFullVisCmd
{
	if (!string_fullvis[0])
		return NULL;
	return string_fullvis;
}

//
//  Return the FASTVIS cmd string
//
-(const char *) getFastVisCmd
{
	if (!string_fastvis[0])
		return NULL;
	return string_fastvis;
}

//
//  Return the NOVIS cmd string
//
-(const char *) getNoVisCmd
{
	if (!string_novis[0])
		return NULL;
	return string_novis;
}

//
//  Return the RELIGHT cmd string
//
-(const char *) getRelightCmd
{
	if (!string_relight[0])
		return NULL;
	return string_relight;
}

//
//  Return the LEAKTEST cmd string
//
-(const char *) getLeaktestCmd
{
	if (!string_leaktest[0])
		return NULL;
	return string_leaktest;
}

-(const char *) getEntitiesCmd
{
	if (!string_entities[0])
		return NULL;
	return string_entities;
}

@end
//====================================================
// C Functions
//====================================================
//
// Change a character to a different char in a string
//
	void
changeString (char cf, char ct, char *string)
{
	int         j;

	for (j = 0; j < strlen (string); j++)
		if (string[j] == cf)
			string[j] = ct;
}
