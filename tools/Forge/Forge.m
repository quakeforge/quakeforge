#import "qedefs.h"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#import "Forge.h"

#include <sys/types.h>
#include <signal.h>

id		quakeed_i;
id		entclasses_i;

id		g_cmd_out_i;

BOOL	autodirty;
BOOL	filter_light, filter_path, filter_entities;
BOOL	filter_clip_brushes, filter_water_brushes, filter_world;

BOOL	running;

int bsppid;

#if 0
// example command strings

char	*fullviscmd = "rsh satan \"/LocalApps/qbsp $1 $2 ; /LocalApps/light $2 ; /LocalApps/vis $2\"";
char	*fastviscmd = "rsh satan \"/LocalApps/qbsp $1 $2 ; /LocalApps/light $2 ; /LocalApps/vis -fast $2\"";
char	*noviscmd = "rsh satan \"/LocalApps/qbsp $1 $2 ; /LocalApps/light $2\"";
char	*relightcmd = "rsh satan \"/LocalApps/light $2\"";
char	*leakcmd = "rsh satan \"/LocalApps/qbsp -mark -notjunc $1 $2\"";
#endif

void NopSound (void)
{
	NSBeep ();
}

UserPath	*upath;


void My_Malloc_Error (int code)
{
// recursive toast	Error ("Malloc error: %i\n", code);
	write (1, "malloc error!\n", strlen("malloc error!\n")+1);
}

/*
===============
AutoSave

Every five minutes, save a modified map
===============
*/
#if 0
void AutoSave(DPSTimedEntry tag, double now, void *userData)
{
	// automatic backup
	if (autodirty) {
		autodirty = NO;
		[map_i writeMapFile: FN_AUTOSAVE useRegion: NO];
	}
	[map_i writeStats];
}
#endif

void DisplayCmdOutput (void)
{
	NSString	*output;

	output = [NSString stringWithContentsOfFile: commandOutput];
	unlink ([commandOutput cString]);
	[project_i addToOutput: output];
	[output release];

	if ([preferences_i getShowBSP])
		[inspcontrol_i changeInspectorTo: i_output];

	[preferences_i playBspSound];		
	
//	NSPing ();
}

/*
===============
CheckCmdDone

See if the BSP is done
===============
*/
#if 0
DPSTimedEntry	cmdte;
void CheckCmdDone(DPSTimedEntry tag, double now, void *userData)
{
    union wait statusp;
    struct rusage rusage;
	
	if (!wait4(bsppid, &statusp, WNOHANG, &rusage))
		return;
	DisplayCmdOutput ();
	bsppid = 0;
	DPSRemoveTimedEntry( cmdte );	
}
#endif
//============================================================================

@implementation Forge

/*
===============
init
===============
*/
- initWithContentRect: (NSRect) contentRect
		styleMask: (unsigned int) styleMask
		backing: (NSBackingStoreType) backingType
		defer: (BOOL) flag
{
	[super initWithContentRect: contentRect
			styleMask: styleMask
			backing: backingType
			defer: flag];

//	[self addToEventMask: NSRightMouseDraggedMask | NSLeftMouseDraggedMask];	
	
    malloc_error (My_Malloc_Error);
	
	quakeed_i = self;
	dirty = autodirty = NO;

//	DPSAddTimedEntry(5*60, AutoSave, self, NX_BASETHRESHOLD);

	upath = newUserPath ();

	return self;
}

- setDefaultFilename
{
	if (!filename) {
		filename = [NSMutableString stringWithString: tempSaveFile];
	} else {
		[filename setString: tempSaveFile];
	}

	[self setTitleWithRepresentedFilename: filename];
	
	return self;
}


- (BOOL)dirty
{
	return dirty;
}

/*
===============================================================================

				DISPLAY UPDATING (handles both camera and XYView)

===============================================================================
*/

BOOL	updateinflight;

BOOL	clearinstance;

BOOL	updatexy;
BOOL	updatez;
BOOL	updatecamera;

- (void) postAppDefined
{
	NSEvent *ev;

	if (updateinflight)
		return;

	ev = [NSEvent otherEventWithType: NSApplicationDefined
			location: (NSPoint) {0,0} modifierFlags: 0
			timestamp: [[NSDate date] timeIntervalSince1970]
			windowNumber: [self windowNumber]
			context: nil subtype: 0 data1: 0 data2: 0];

	// post an event at the end of the queue
	DPSPostEvent(nil, ev, 0);
//	printf ("posted\n");
	updateinflight = YES;
}


int	c_updateall;
- updateAll			// when a model has been changed
{
	updatecamera = updatexy = updatez = YES;
	c_updateall++;
	[self postAppDefined];
	return self;
}

- updateAll:sender
{
	[self updateAll];
	return self;
}

- updateCamera		// when the camera has moved
{
	updatecamera = YES;
	clearinstance = YES;
	
	[self postAppDefined];
	return self;
}

- updateXY
{
	updatexy = YES;
	[self postAppDefined];
	return self;
}

- updateZ
{
	updatez = YES;
	[self postAppDefined];
	return self;
}


- newinstance
{
	clearinstance = YES;
	return self;
}

- redrawInstance
{
	clearinstance = YES;
	[self flushWindow];
	return self;
}

/*
===============
flushWindow

instance draw the brush after each flush
===============
*/
- (void) flushWindow
{
	[super flushWindow];
	
	if (!running || in_error) // don't lock focus before nib is loaded
		return; 		
		
	if ([self isFlushWindowDisabled])
		return;
		
	[cameraview_i lockFocus];	

	if (clearinstance) {
		PSnewinstance ();
		clearinstance = NO;
	}

	PSsetinstance (1);
	linestart (0,0,0);
	[map_i makeSelectedPerform: @selector(CameraDrawSelf)];
	[clipper_i cameraDrawSelf];
	lineflush ();
	PSsetinstance (0);
	[cameraview_i unlockFocus];	

	[xyview_i lockFocus];
	PSsetinstance (1);
	linestart (0,0,0);
	[map_i makeSelectedPerform: @selector(XYDrawSelf)];
	lineflush ();
	[cameraview_i XYDrawSelf];
	[zview_i XYDrawSelf];
	[clipper_i XYDrawSelf];
	PSsetinstance (0);
	[xyview_i unlockFocus];

	[zview_i lockFocus];
	PSsetinstance (1);
	[map_i makeSelectedPerform: @selector(ZDrawSelf)];
	[cameraview_i ZDrawSelf];
	[clipper_i ZDrawSelf];
	PSsetinstance (0);
	[zview_i unlockFocus];

	return;
}


/*
==============================================================================

App delegate methods

==============================================================================
*/

- applicationDefined: (NSEvent *) theEvent
{
	NSEvent 	*ev;
	
	updateinflight = NO;

//printf ("serviced\n");
	
// update screen	
	ev = [NSApp peekNextEvent: -1];
	if (ev) {
		[self postAppDefined];
		return self;
	}
		
	[self disableFlushWindow];	

	if ([map_i count] != [entitycount_i intValue])
		[entitycount_i setIntValue: [map_i count]];
	if ([[map_i currentEntity] count] != [brushcount_i intValue])
		[brushcount_i setIntValue: [[map_i currentEntity] count]];
		
	if (updatecamera)
		[cameraview_i display];
	if (updatexy)
		[xyview_i display];
	if (updatez)
		[zview_i display];

	updatecamera = updatexy = updatez = NO;

	[self enableFlushWindow];
	[self flushWindow];
	
//	NXPing ();
	
	return self;
}

- appDidInitialize: sender
{
#if 0
	NSScreen	const *screens;
	int			screencount;
#endif
	
	running = YES;
	g_cmd_out_i = cmd_out_i;	// for qprintf

	[preferences_i	readDefaults];
	[project_i		initProject];

	[xyview_i setModeRadio: xy_drawmode_i];	// because xy view is inside
											// scrollview and can't be
											// connected directly in IB
	
	[self setFrameAutosaveName: @"EditorWinFrame"];
	[self clear: self];

#if 0
// go to my second monitor
	[NSApp getScreens:&screens count:&screencount];
	if (screencount == 2)
		[self moveTopLeftTo:0 : screens[1].screenBounds.size.height
		screen:screens+1];
#endif
	
	[self makeKeyAndOrderFront: self];

//[self doOpen: "/raid/quake/id1_/maps/amlev1.map"];	// DEBUG
	[map_i newMap];
		
	qprintf ("ready.");

//malloc_debug(-1);		// DEBUG
	
	return self;
}

- appWillTerminate:sender
{
// FIXME: save dialog if dirty
	return self;
}


//===========================================================================

- textCommand: sender
{
	NSString	*t;
	
	t = [sender stringValue];
	
	if ([t isEqualToString: @"texname"]) {
		texturedef_t	*td;
		id				b;
		
		b = [map_i selectedBrush];
		if (!b)
		{
			qprintf ("nothing selected");
			return self;
		}
		td = [b texturedef];
		qprintf (td->texture);
		return self;
	}
	else
		qprintf ("Unknown command\n");
	return self;
}


- openProject:sender
{
	[project_i	openProject];
	return self;
}


- clear: sender
{	
	[map_i newMap];

	[self updateAll];
	[regionbutton_i setIntValue: 0];
	[self setDefaultFilename];

	return self;
}


- centerCamera: sender
{
	NSRect	sbounds = [[xyview_i superview] bounds];

	sbounds.origin.x += sbounds.size.width/2;
	sbounds.origin.y += sbounds.size.height/2;
	
	[cameraview_i setXYOrigin: sbounds.origin];
	[self updateAll];
	
	return self;
}

- centerZChecker: sender
{
	NSRect	sbounds = [[xyview_i superview] bounds];
	
	sbounds.origin.x += sbounds.size.width/2;
	sbounds.origin.y += sbounds.size.height/2;
	
	[zview_i setPoint: sbounds.origin];
	[self updateAll];
	
	return self;
}

- changeXYLookUp: sender
{
	if ([sender intValue])
	{
		xy_viewnormal[2] = 1;
	}
	else
	{
		xy_viewnormal[2] = -1;
	}
	[self updateAll];
	return self;
}

/*
==============================================================================

REGION MODIFICATION

==============================================================================
*/


/*
==================
applyRegion:
==================
*/
- applyRegion: sender
{
	filter_clip_brushes = [filter_clip_i intValue];
	filter_water_brushes = [filter_water_i intValue];
	filter_light = [filter_light_i intValue];
	filter_path = [filter_path_i intValue];
	filter_entities = [filter_entities_i intValue];
	filter_world = [filter_world_i intValue];

	if (![regionbutton_i intValue])
	{
		region_min[0] = region_min[1] = region_min[2] = -9999;
		region_max[0] = region_max[1] = region_max[2] = 9999;
	}

	[map_i makeGlobalPerform: @selector(newRegion)];
	
	[self updateAll];

	return self;
}

- setBrushRegion: sender
{
	id		b;

// get the bounds of the current selection
	
	if ([map_i numSelected] != 1)
	{
		qprintf ("must have a single brush selected");
		return self;
	} 

	b = [map_i selectedBrush];
	[b getMins: region_min maxs: region_max];
	[b remove];

// turn region on
	[regionbutton_i setIntValue: 1];
	[self applyRegion: self];
	
	return self;
}

- setXYRegion: sender
{
	NSRect	bounds = [[xyview_i superview] bounds];

	region_min[0] = bounds.origin.x;
	region_min[1] = bounds.origin.y;
	region_min[2] = -99999;
	region_max[0] = bounds.origin.x + bounds.size.width;
	region_max[1] = bounds.origin.y + bounds.size.height;
	region_max[2] = 99999;
	
	// turn region on
	[regionbutton_i setIntValue: 1];
	[self applyRegion: self];
	
	return self;
}

//
// UI querie for other objects
//
- (BOOL)showCoordinates
{
	return [show_coordinates_i intValue];
}

- (BOOL)showNames
{
	return [show_names_i intValue];
}


/*
==============================================================================

BSP PROCESSING

==============================================================================
*/

NSString *
ExpandCommand (NSString *input, NSString *src, NSString *dest)
{
	return [[[[input
					componentsSeparatedByString: @"$1"]
					componentsJoinedByString: src]
					componentsSeparatedByString: @"$2"]
					componentsJoinedByString: dest];
}


/*
=============
saveBSP
=============
*/
- saveBSP: (NSString *) cmdline dialog: (BOOL) wt
{
	int			oldLightFilter;
	int			oldPathFilter;
	NSString	*expandedcmd;
	NSString	*mappath;
	NSString 	*bsppath;
	NSString	*destdir;
	
	if (bsppid) {
		NSBeep();
		return self;
	}

//
// turn off the filters so all entities get saved
//
	oldLightFilter = [filter_light_i intValue];
	oldPathFilter = [filter_path_i intValue];
	[filter_light_i setIntValue:0];
	[filter_path_i setIntValue:0];
	[self applyRegion: self];
	
	if ([regionbutton_i intValue]) {
		mappath = [[filename
						stringByDeletingPathExtension]
						stringByAppendingPathExtension: @"reg"];
		[map_i writeMapFile: mappath useRegion: YES];
		wt = YES;		// always pop the dialog on region ops
	} else {
		mappath = filename;
	}
		
// save the entire thing, just in case there is a problem
	[self save: self];

	[filter_light_i setIntValue:oldLightFilter];
	[filter_path_i setIntValue:oldPathFilter];
	[self applyRegion: self];

//
// write the command to the bsp host
//	
	destdir = [project_i finalMapDirectory];

	bsppath = [[destdir
					stringByAppendingString: @"/"]
					stringByAppendingString: [[[mappath lastPathComponent]
						stringByDeletingPathExtension]
						stringByAppendingPathExtension: @"bsp"]];
	
	expandedcmd = ExpandCommand (cmdline, mappath, bsppath);

	NSLog (@"system: %s", expandedcmd);
	
	[project_i addToOutput: @"\n\n========= BUSY =========\n\n"];
	[project_i addToOutput: expandedcmd];

	if ([preferences_i getShowBSP])
		[inspcontrol_i changeInspectorTo:i_output];
	
	if (wt) {
		id	panel;
		
		panel = NSGetAlertPanel (@"BSP In Progress", expandedcmd, NULL, NULL, NULL);
	
		[panel makeKeyAndOrderFront: NULL];
		system ([expandedcmd cString]);
		NSReleaseAlertPanel (panel);
		[self makeKeyAndOrderFront: NULL];
	
		DisplayCmdOutput ();
	} else {
//		cmdte = DPSAddTimedEntry(1, CheckCmdDone, self, NX_BASETHRESHOLD);
		if (!(bsppid = fork ())) {
			system ([expandedcmd cString]);
			exit (0);
		}
	}
	
	return self;
}


- BSP_Full: sender
{
	[self saveBSP: [project_i fullVisCommand] dialog: NO];
	return self;
}

- BSP_FastVis: sender
{
	[self saveBSP: [project_i fastVisCommand] dialog: NO];
	return self;
}

- BSP_NoVis: sender
{
	[self saveBSP: [project_i noVisCommand] dialog: NO];
	return self;
}

- BSP_relight: sender
{
	[self saveBSP: [project_i relightCommand] dialog: NO];
	return self;
}

- BSP_entities: sender
{
	[self saveBSP: [project_i entitiesCommand] dialog: NO];
	return self;
}

- BSP_stop: sender
{
	if (!bsppid) {
		NSBeep ();
		return self;
	}
	
	kill (bsppid, 9);
//	CheckCmdDone (cmdte, 0, NULL);
	[project_i addToOutput: @"\n\n========= STOPPED =========\n\n"];
	
	return self;
}



/*
==============
doOpen:

Called by open or the project panel
==============
*/
- doOpen: (NSString *)fname;
{	
	[filename setString: fname];
	
	[map_i readMapFile: filename];
	
	[regionbutton_i setIntValue: 0];
	[self setTitleWithRepresentedFilename: filename];
	[self updateAll];

	qprintf ("%s loaded\n", fname);
	
	return self;
}


/*
==============
open
==============
*/
- open: sender;
{
	id	openPanel = [NSOpenPanel openPanel];
	id	fileTypes = [NSArray arrayWithObject: @"map"];

	if ([openPanel
			runModalForDirectory: [project_i mapDirectory]
			file: @""
			types: fileTypes] != NSOKButton)
		return self;

	[self doOpen: [openPanel filename]];
	
	return self;
}


/*
==============
save:
==============
*/
- save: sender;
{
	NSString 	*backup;

	// force a name change if using tempname
	if ([filename isEqualToString: tempSaveFile])
		return [self saveAs: self];
		
	dirty = autodirty = NO;

	backup = [[filename	stringByDeletingPathExtension]
				stringByAppendingPathExtension: @"bak"];

	rename ([filename cString], [backup cString]);		// copy old to .bak

	[map_i writeMapFile: filename useRegion: NO];

	return self;
}


/*
	saveAs
*/
- saveAs: sender;
{
	id			panel_i = [NSSavePanel savePanel];
	NSString	*dir;
	
	dir = [[filename lastPathComponent] stringByDeletingPathExtension];
	
	[panel_i setRequiredFileType: @"map"];
	if ([panel_i runModalForDirectory: [project_i mapDirectory] file: dir] != NSOKButton)
		return self;
	
	[filename setString: [panel_i filename]];
	
	[self setTitleWithRepresentedFilename: filename];
	
	[self save: self];	
	
	return self;
}


/*
===============================================================================

						OTHER METHODS

===============================================================================
*/


//
//	AJR - added this for Project info
//
- (NSString *) currentFilename
{
	return filename;
}

- deselect: sender
{
	if ([clipper_i hide])	// first click hides clipper only
		return [self updateAll];

	[map_i setCurrentEntity: [map_i objectAtIndex: 0]];	// make world selected
	[map_i makeSelectedPerform: @selector(deselect)];
	[self updateAll];
	
	return self;
}


/*
===============
keyDown
===============
*/

#define	KEY_RIGHTARROW		0xae
#define	KEY_LEFTARROW		0xac
#define	KEY_UPARROW			0xad
#define	KEY_DOWNARROW		0xaf

- (void) keyDown:(NSEvent *)theEvent
{
	// function keys
	switch ([[theEvent characters] characterAtIndex: 0]) {
		case NSF2FunctionKey:	// F2
			[cameraview_i setDrawMode: dr_wire];
			qprintf ("wire draw mode");
			return;
		case NSF3FunctionKey:	// F3
			[cameraview_i setDrawMode: dr_flat];
			qprintf ("flat draw mode");
			return;
		case NSF4FunctionKey:	// F4
			[cameraview_i setDrawMode: dr_texture];
			qprintf ("texture draw mode");
			return;
		case NSF5FunctionKey:	// F5
			[xyview_i setDrawMode: dr_wire];
			qprintf ("wire draw mode");
			return;
		case NSF6FunctionKey:	// F6
			qprintf ("texture draw mode");
			return;

		case NSF8FunctionKey:	// F8
			[cameraview_i homeView: self];
			return;
		
		case NSF12FunctionKey:	// F12
			[map_i subtractSelection: self];
			return;

		case NSPageUpFunctionKey:	// page up
			[cameraview_i upFloor: self];
			return;
		case NSPageDownFunctionKey:	// page down
			[cameraview_i downFloor: self];
			return;

		case NSEndFunctionKey:	// end
			[self deselect: self];
			return;
	}

	// portable things
	switch ([[[theEvent characters] lowercaseString] characterAtIndex: 0]) {
		case NSRightArrowFunctionKey:
		case NSLeftArrowFunctionKey:
		case NSUpArrowFunctionKey:
		case NSDownArrowFunctionKey:
		case 'a':
		case 'z':
		case 'd':
		case 'c':
		case '.':
		case ',':
			[cameraview_i _keyDown: theEvent];
			break;

		case 27:	// escape
			autodirty = dirty = YES;
			[self deselect: self];
			return;
		
		case NSDeleteFunctionKey:	// delete
			autodirty = dirty = YES;
			[map_i makeSelectedPerform: @selector(remove)];
			[clipper_i hide];
			[self updateAll];
			break;

		case '/':
			[clipper_i flipNormal];
			[self updateAll];
			break;

		case 13:	// enter
			[clipper_i carve];
			[self updateAll];
			qprintf ("carved brush");
			break;

		case ' ':
			[map_i cloneSelection: self];
			break;

//
// move selection keys
//		
		case '2':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[1] = -[xyview_i gridsize];
			[map_i makeSelectedPerform: @selector(translate)];
			[self updateAll];
			break;

		case '8':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[1] = [xyview_i gridsize];
			[map_i makeSelectedPerform: @selector(translate)];
			[self updateAll];
			break;

		case '4':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[0] = -[xyview_i gridsize];
			[map_i makeSelectedPerform: @selector(translate)];
			[self updateAll];
			break;

		case '6':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[0] = [xyview_i gridsize];
			[map_i makeSelectedPerform: @selector(translate)];
			[self updateAll];
			break;

		case '-':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[2] = -[xyview_i gridsize];
			[map_i makeSelectedPerform: @selector(translate)];
			[self updateAll];
			break;
		case '+':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[2] = [xyview_i gridsize];
			[map_i makeSelectedPerform: @selector(translate)];
			[self updateAll];
			break;

		default:
			qprintf ("undefined keypress");
			NopSound ();
			break;
	}

    return;
}


@end
