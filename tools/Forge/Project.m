//======================================
//
// QuakeEd Project Management
//
//======================================

#import "qedefs.h"

#import "Preferences.h"

Project *	project;

@implementation Project

- init
{
	project = self;

	return self;
}

//===========================================================
//
//	Project code
//
//===========================================================
- initVars
{
	NSString	*s = [[Preferences sharedInstance] objectForKey: ProjectPath];

	_basePath = [s stringByDeletingLastPathComponent];

	_progsPath = [_basePath stringByAppendingPathComponent: EntSubdir];
	_mapPath = [_basePath stringByAppendingPathComponent: MapSubdir];
	_finalMapPath = [_basePath stringByAppendingPathComponent: MapSubdir];

	[pBasePathField setStringValue: s];		// in Project Inspector

	if ((s = [projectInfo objectForKey: BSPFullVis])) {
		_fullVisCommand = [[[s componentsSeparatedByString: @"@"]
								componentsJoinedByString: @"\""] retain];
	}
		
	if ((s = [projectInfo objectForKey: BSPFastVis])) {
		_fastVisCommand = [[[s componentsSeparatedByString: @"@"]
								componentsJoinedByString: @"\""] retain];
	}
		
	if ((s = [projectInfo objectForKey: BSPNoVis])) {
		_noVisCommand = [[[s componentsSeparatedByString: @"@"]
								componentsJoinedByString: @"\""] retain];
	}
		
	if ((s = [projectInfo objectForKey: BSPRelight])) {
		_relightCommand = [[[s componentsSeparatedByString: @"@"]
								componentsJoinedByString: @"\""] retain];
	}
		
	if ((s = [projectInfo objectForKey: BSPLeakTest])) {
		_leakTestCommand = [[[s componentsSeparatedByString: @"@"]
								componentsJoinedByString: @"\""] retain];
	}

	if ((s = [projectInfo objectForKey: BSPEntities])) {
		_entitiesCommand = [[[s componentsSeparatedByString: @"@"]
								componentsJoinedByString: @"\""] retain];
	}

	// Build list of wads
	wadNames = [projectInfo objectForKey: WadFiles];

	//	Build list of maps & descriptions
	mapNames = [projectInfo objectForKey: MapNames];
	descriptions = [projectInfo objectForKey: Descriptions];
	[self changeString: @"_" to: @" " in: descriptions];
	
	[self initProjectSettings];

	return self;
}

- (void) initProjectSettings
{
	[ppBasePath 		setStringValue: _basePath];
	[ppFullVisField 	setStringValue: _fullVisCommand];
	[ppFullVisField 	setStringValue: _fastVisCommand];
	[ppNoVisField		setStringValue: _noVisCommand];
	[ppRelightField 	setStringValue: _relightCommand];
	[ppLeakTestField	setStringValue: _leakTestCommand];
	
	return;
}

//
//	Add text to the BSP Output window
//
- (void) addToOutput: (NSString *) string
{
	NSRange range = NSMakeRange([pBSPOutputView textLength], 0);

	[pBSPOutputView replaceCharactersInRange: range withString: string];
	
	range = NSMakeRange([pBSPOutputView textLength], 0);
	[pBSPOutputView scrollRangeToVisible: range];
	
	return;
}

- (void) clearBspOutput: (id) sender
{
	[pBSPOutputView selectAll: self];
	[pBSPOutputView delete: self];
	
	return;
}

//- (void) print
//{
//	[pBSPOutputView printPSCode: self];
//	return;
//}


- initProject
{
	[self parseProjectFile];
	if (!projectInfo)
		return self;

	[self initVars];
//	[pMapBrowser reuseColumns: YES];
	[pMapBrowser loadColumnZero];
//	[ppWadBrowser reuseColumns: YES];
	[ppWadBrowser loadColumnZero];

	[things_i		initEntities];
	
	return self;
}

//
//	Change a character to another in a Storage list of strings
//
- (void) changeString: (NSString *) f to: (NSString *) t in: (id) obj
{
	NSMutableArray	*newObj = [[NSMutableArray alloc] initWithCapacity: 10];
	NSEnumerator	*enumerator = [obj objectEnumerator];
	id				currentString;

	while ((currentString = [enumerator nextObject])) {
		[newObj addObject: [[currentString componentsSeparatedByString: f]
											componentsJoinedByString: t]];
		[currentString release];
	}

	[obj release];
	obj = newObj;
}

//	Fill the Maps or Wads browser
//	(Delegate method - delegated in Interface Builder)
- (int) browser: (id) sender fillMatrix: (id) matrix inColumn: (int) column
{
	NSString	*name;
	NSArray 	*list;
	NSCell		*cell;
	int			max;
	int			i;

	if (sender == pMapBrowser) {
		list = mapNames;
	} else {
		if (sender == ppWadBrowser) {
			list = wadNames;
		} else {
			list = nil;
			NSLog (@"Project: Unknown browser to fill");
		}
	}

	max = [list count];
	for (i = 0; i < max; i++) {
		name = [[list objectAtIndex: i] stringValue];
		[matrix addRow];
		cell = [matrix cellAtRow: i column: 0];
		[cell setStringValue: name];
//		[cell setLeaf: YES];
//		[cell setLoaded: YES];
	}
	return i;
}

//	Clicked on a map name or description!
- (void) mapWasClicked: (id) sender
{
	NSString	*fname;
	id	matrix;
	int	row;
	id	panel;
	
	matrix = [sender matrixInColumn: 0];
	row = [matrix selectedRow];

	fname = [_mapPath stringByAppendingPathComponent:
				[[mapNames objectAtIndex: row]
					stringByAppendingPathExtension: @"map"
				]
			];
	
	panel = NSGetInformationalAlertPanel (@"Loading...",
		@"Loading map. Please wait...", nil, nil, nil);
	[panel orderFront: self];
	[quakeed_i doOpen: fname];
	[panel performClose: self];
	NSReleaseAlertPanel (panel);

	return;
}


- (void) setTextureWad: (NSString *) wadFile
{
	int			i, c;
	NSString	*name;
	
	NSLog (@"loading %s", wadFile);

	// set the row in the settings inspector wad browser
	c = [wadNames count];
	for (i = 0; i < c; i++) {
		name = [[wadNames objectAtIndex: i] stringValue];
		if ([name isEqualToString: wadFile]) {
			[ppWadBrowser selectCellAtRow: i column: 0];
			break;
		}
	}

	// update the texture inspector
	[texturepalette_i initPaletteFromWadfile: wadFile];
	[[map_i objectAtIndex: 0] setObject: wadFile forKey: @"wad"];
//	[inspcontrol_i changeInspectorTo:i_textures];

	[quakeed_i updateAll];

	return;
}

//
//	Clicked on a wad name
//
- (void) wadWasClicked: (id) sender
{
	id			matrix;
	int			row;
	NSString	*name;
	
	matrix = [sender matrixInColumn: 0];
	row = [matrix selectedRow];

	name = [wadNames objectAtIndex: row];
	[self setTextureWad: name];
	
	return;
}


//
//	Read in the <name>.QE_Project file
//
- (void) parseProjectFile
{
	NSString	*path;
	int			rtn;
	
	if (!(path = [[Preferences sharedInstance] objectForKey: ProjectPath])) {
		rtn = NSRunAlertPanel (@"Project Error",
			@"A default project has not been found.\n"
			, @"Open Project", nil, nil);
		if ([self openProject] == nil)
			while (1)		// can't run without a project
				[NSApp terminate: self];
		return;	
	}

	[self openProjectWithFile: path];
	return;
}

//
//	Loads and parses a project file
//
- (void) openProjectWithFile: (NSString *) path
{		
	[_projectPath release];
	_projectPath = path;

	[projectInfo release];
	
	projectInfo = [NSMutableDictionary dictionaryWithContentsOfFile: path];
	
	return;
}

//	Open a project file
- (id) openProject
{
	int 		result;
	NSArray 	*fileTypes = [NSArray arrayWithObject: @"forge"];
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];

	[openPanel setAllowsMultipleSelection: NO];
	[openPanel setCanChooseDirectories: NO];

	result = [openPanel runModalForTypes: fileTypes];
	if (result == NSOKButton) {
		NSArray 	*filenames = [[openPanel filenames] autorelease];
		NSString	*directory = [[openPanel directory] autorelease];
		NSString	*path = [directory stringByAppendingPathComponent: [filenames objectAtIndex: 0]];

		[self openProjectWithFile: path];

		return self;
	}

	return nil;
}


//
//	Search for a string in an array
//
- (BOOL) searchForString: (NSString *) str in: (NSArray *) obj
{
	NSEnumerator	*enumerator = [obj objectEnumerator];
	id				name;

	while ((name = [enumerator nextObject])) {
		if ([[name stringValue] isEqualToString: str])
			return YES;
	}
	return NO;
}

- (NSString *) currentProject
{
	return _projectPath;
}

- (NSString *) mapDirectory
{
	return _mapPath;
}

- (NSString *) finalMapDirectory
{
	return _finalMapPath;
}

- (NSString *) progDirectory
{
	return _progsPath;
}

- (NSString *) wad8
{
	return _wad8 ? _wad8 : nil;
}

- (NSString *) wad9
{
	return _wad9 ? _wad9 : nil;
}

- (NSString *) wad0
{
	return _wad0 ? _wad0 : nil;
}

- (NSString *) fullVisCommand
{
	return _fullVisCommand ? _fullVisCommand : nil;
}

- (NSString *) fastVisCommand
{
	return _fastVisCommand ? _fastVisCommand : nil;
}

- (NSString *) noVisCommand
{
	return _noVisCommand ? _noVisCommand : nil;
}

- (NSString *) relightCommand
{
	return _relightCommand ? _relightCommand : nil;
}

- (NSString *) leakTestCommand
{
	return _leakTestCommand ? _leakTestCommand : nil;
}

- (NSString *) entitiesCommand
{
	return _entitiesCommand ? _entitiesCommand : nil;
}

@end

//====================================================
// C Functions
//====================================================

//
// Change a character to a different char in a string
//
void changeString(char cf,char ct,char *string)
{
	int	j;

	for (j = 0;j < strlen(string);j++)
		if (string[j] == cf)
			string[j] = ct;
}


