#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "QF/quakeio.h"
#include "QF/sys.h"

#include "QuakeEd.h"
#include "Clipper.h"
#include "XYView.h"
#include "Map.h"
#include "CameraView.h"
#include "ZView.h"
#include "Preferences.h"
#include "InspectorControl.h"
#include "Project.h"

id          quakeed_i;
id          entclasses_i;

extern NSBezierPath *path;
id          g_cmd_out_i;

BOOL        autodirty;
BOOL        filter_light, filter_path, filter_entities;
BOOL        filter_clip_brushes, filter_water_brushes, filter_world;

BOOL        running;

int         bsppid;

#if 0
// example command strings

char       *fullviscmd =
	"rsh satan \"/LocalApps/qbsp $1 $2 ; /LocalApps/light $2 ; /LocalApps/vis $2\"";
char       *fastviscmd =
	"rsh satan \"/LocalApps/qbsp $1 $2 ; /LocalApps/light $2 ; /LocalApps/vis -fast $2\"";
char       *noviscmd =
	"rsh satan \"/LocalApps/qbsp $1 $2 ; /LocalApps/light $2\"";
char       *relightcmd = "rsh satan \"/LocalApps/light $2\"";
char       *leakcmd = "rsh satan \"/LocalApps/qbsp -mark -notjunc $1 $2\"";
#endif

void
NopSound (void)
{
	NSBeep ();
}


void
My_Malloc_Error (int code)
{
// recursive toast  Error ("Malloc error: %i\n", code);
	write (1, "malloc error!\n", strlen ("malloc error!\n") + 1);
}

#define        FN_CMDOUT               "/tmp/QuakeEdCmd.txt"
void
DisplayCmdOutput (void)
{
	char       *buffer;
	QFile      *file;
	int         size;

	file = Qopen (FN_CMDOUT, "rt");
	size = Qfilesize (file);
	buffer = malloc (size + 1);
	size = Qread (file, buffer, size);
	Qclose (file);
	unlink (FN_CMDOUT);
	[project_i addToOutput:buffer];
	free (buffer);

	if ([preferences_i getShowBSP])
		[inspcontrol_i changeInspectorTo:i_output];

	[preferences_i playBspSound];

}

/*
===============
CheckCmdDone

See if the BSP is done
===============
*/
//DPSTimedEntry cmdte;
void
CheckCmdDone ( /* DPSTimedEntry tag, */ double now, void *userData)
{
	union wait  statusp;
	struct rusage rusage;

	if (!wait4 (bsppid, &statusp, WNOHANG, &rusage))
		return;
	DisplayCmdOutput ();
	bsppid = 0;
//  DPSRemoveTimedEntry( cmdte );   
}

//============================================================================

@implementation QuakeEd
/*
===============
AutoSave

Every five minutes, save a modified map
===============
*/
- (void) AutoSave {
// automatic backup
	if (autodirty) {
		autodirty = NO;
#define FN_AUTOSAVE "/qcache/AutoSaveMap.map"
		[map_i writeMapFile: FN_AUTOSAVE useRegion:NO];
	}
	[map_i writeStats];
}


#define FN_TEMPSAVE "/qcache/temp.map"
-setDefaultFilename
{
	strcpy (filename, FN_TEMPSAVE);
	[self setTitleWithRepresentedFilename: [NSString stringWithCString:filename]];

	return self;
}


-(BOOL) dirty
{
	return dirty;
}

/*
===============================================================================

				DISPLAY UPDATING (handles both camera and XYView)

===============================================================================
*/

BOOL        updateinflight;

BOOL        clearinstance;

BOOL        updatexy;
BOOL        updatez;
BOOL        updatecamera;

void
postappdefined (void)
{
	NSEvent    *ev;

	if (updateinflight)
		return;

// post an event at the end of the que
	ev =[NSEvent otherEventWithType: NSApplicationDefined location: NSZeroPoint modifierFlags: 0 timestamp:[[NSDate date]
	 timeIntervalSinceReferenceDate]
	windowNumber: 0 context:[NSApp context]
	subtype: 0 data1: 0 data2:0];
	[NSApp postEvent: ev atStart:NO];
	Sys_Printf ("posted\n");
	updateinflight = YES;
}


int         c_updateall;

-updateAll								// when a model has been changed
{
	updatecamera = updatexy = updatez = YES;
	c_updateall++;
	postappdefined ();
	return self;
}

-updateAll:sender
{
	[self updateAll];
	return self;
}

-updateCamera							// when the camera has moved
{
	updatecamera = YES;
	clearinstance = YES;

	postappdefined ();
	return self;
}

-updateXY
{
	updatexy = YES;
	postappdefined ();
	return self;
}

-updateZ
{
	updatez = YES;
	postappdefined ();
	return self;
}


-newinstance
{
	clearinstance = YES;
	return self;
}

-redrawInstance
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
-flushWindow
{
	[super flushWindow];

	if (!running)
		return self;					// don't lock focus before nib is
										// finished loading

	// if (_flushDisabled)
	// return self;

	[cameraview_i lockFocus];
	if (clearinstance) {
		// XXX PSnewinstance ();
		clearinstance = NO;
	}
	// XXX PSsetinstance (1);
	linestart (0, 0, 0);
	[map_i makeSelectedPerform:@selector (CameraDrawSelf)];
	[clipper_i cameraDrawSelf];
	lineflush ();
	// XXX PSsetinstance (0);
	[cameraview_i unlockFocus];

	[xyview_i lockFocus];
	// XXX PSsetinstance (1);
	linestart (0, 0, 0);
	[map_i makeSelectedPerform:@selector (XYDrawSelf)];
	lineflush ();
	[cameraview_i XYDrawSelf];
	[zview_i XYDrawSelf];
	[clipper_i XYDrawSelf];
	// XXX PSsetinstance (0);
	[xyview_i unlockFocus];

	[zview_i lockFocus];
	// XXX PSsetinstance (1);
	[map_i makeSelectedPerform:@selector (ZDrawSelf)];
	[cameraview_i ZDrawSelf];
	[clipper_i ZDrawSelf];
	// XXX PSsetinstance (0);
	[zview_i unlockFocus];

	return self;
}


/*
==============================================================================

App delegate methods

==============================================================================
*/

-applicationDefined:(NSEvent *) theEvent
{
	NSEvent    *evp;

	updateinflight = NO;

	Sys_Printf ("serviced\n");

// update screen    
	evp = [NSApp nextEventMatchingMask: NSAnyEventMask
							 untilDate: [NSDate distantPast]
								inMode: NSEventTrackingRunLoopMode
							   dequeue: NO];
	if (evp) {
		postappdefined ();
		return self;
	}
	Sys_Printf ("updating %d %d\n", (int)[map_i count], (int)[[map_i currentEntity] count]);


	[self disableFlushWindow];

	if ([map_i count] != [entitycount_i intValue])
		[entitycount_i setIntValue:[map_i count]];
	if ([[map_i currentEntity] count] != [brushcount_i intValue])
		[brushcount_i setIntValue:[[map_i currentEntity] count]];

	if (updatecamera)
		[cameraview_i display];
	if (updatexy)
		[xyview_i display];
	if (updatez)
		[zview_i display];

	updatecamera = updatexy = updatez = NO;

	[self enableFlushWindow];
	[self flushWindow];

//  NSPing ();

	return self;
}

-(void)awakeFromNib
{
	// XXX [self addToEventMask:
	// XXX NSRightMouseDragged|NSLeftMouseDragged]; 

	// XXX malloc_error(My_Malloc_Error);

	quakeed_i = self;
	dirty = autodirty = NO;

	[NSTimer timerWithTimeInterval: 5 * 60 target: self selector:@selector
	 (AutoSave)
	userInfo: nil repeats:YES];

	path =[NSBezierPath new];
}

-(void)applicationDidFinishLaunching:(NSNotification *) notification
{
	NSArray    *screens;
	NSScreen   *scrn;

	running = YES;
	g_cmd_out_i = cmd_out_i;			// for qprintf

	[preferences_i readDefaults];
	[project_i initProject];

	[xyview_i setModeRadio:xy_drawmode_i];
	// because xy view is inside
	// scrollview and can't be
	// connected directly in IB

	[self setFrameAutosaveName:@"EditorWinFrame"];
	[self clear:self];

// go to my second monitor
	screens =[NSScreen screens];
	if ([screens count] == 2) {
		scrn =[screens objectAtIndex:1];
		// XXX [self moveTopLeftTo:0 : [scrn frame].size.height
		// XXX screen:scrn];
	}

	[self makeKeyAndOrderFront:self];

//[self doOpen: "/raid/quake/id1_/maps/amlev1.map"];    // DEBUG
	[map_i newMap];

	Sys_Printf ("ready.\n");

//malloc_debug(-1);     // DEBUG
}

-appWillTerminate:sender
{
// FIXME: save dialog if dirty
	return self;
}


//===========================================================================

-textCommand:sender
{
	char const *t;

	t =[[sender stringValue] cString];

	if (!strcmp (t, "texname")) {
		texturedef_t *td;
		id          b;

		b =[map_i selectedBrush];
		if (!b) {
			Sys_Printf ("nothing selected\n");
			return self;
		}
		td =[b texturedef];
		Sys_Printf ("%s\n", td->texture);
		return self;
	} else
		Sys_Printf ("Unknown command\n");
	return self;
}


-openProject:sender
{
	[project_i openProject];
	return self;
}


-clear:sender
{
	[map_i newMap];

	[self updateAll];
	[regionbutton_i setIntValue:0];
	[self setDefaultFilename];

	return self;
}


-centerCamera:sender
{
	NSRect      sbounds;

	sbounds =[[xyview_i superview] bounds];

	sbounds.origin.x += sbounds.size.width / 2;
	sbounds.origin.y += sbounds.size.height / 2;

	[cameraview_i setXYOrigin:&sbounds.origin];
	[self updateAll];

	return self;
}

-centerZChecker:sender
{
	NSRect      sbounds;

	sbounds =[[xyview_i superview] bounds];

	sbounds.origin.x += sbounds.size.width / 2;
	sbounds.origin.y += sbounds.size.height / 2;

	[zview_i setPoint:&sbounds.origin];
	[self updateAll];

	return self;
}

-changeXYLookUp:sender
{
	if ([sender intValue]) {
		xy_viewnormal[2] = 1;
	} else {
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
-applyRegion:sender
{
	filter_clip_brushes =[filter_clip_i intValue];
	filter_water_brushes =[filter_water_i intValue];
	filter_light =[filter_light_i intValue];
	filter_path =[filter_path_i intValue];
	filter_entities =[filter_entities_i intValue];
	filter_world =[filter_world_i intValue];

	if (![regionbutton_i intValue]) {
		region_min[0] = region_min[1] = region_min[2] = -9999;
		region_max[0] = region_max[1] = region_max[2] = 9999;
	}

	[map_i makeGlobalPerform:@selector (newRegion)];

	[self updateAll];

	return self;
}

-setBrushRegion:sender
{
	id          b;

// get the bounds of the current selection

	if ([map_i numSelected] != 1) {
		Sys_Printf ("must have a single brush selected\n");
		return self;
	}

	b =[map_i selectedBrush];
	[b getMins: region_min maxs:region_max];
	[b remove];

// turn region on
	[regionbutton_i setIntValue:1];
	[self applyRegion:self];

	return self;
}

-setXYRegion:sender
{
	NSRect      bounds;

// get xy size
	bounds =[[xyview_i superview] bounds];

	region_min[0] = bounds.origin.x;
	region_min[1] = bounds.origin.y;
	region_min[2] = -99999;
	region_max[0] = bounds.origin.x + bounds.size.width;
	region_max[1] = bounds.origin.y + bounds.size.height;
	region_max[2] = 99999;

// turn region on
	[regionbutton_i setIntValue:1];
	[self applyRegion:self];

	return self;
}

//
// UI querie for other objects
//
-(BOOL) showCoordinates
{
	return[show_coordinates_i intValue];
}

-(BOOL) showNames
{
	return[show_names_i intValue];
}


/*
==============================================================================

BSP PROCESSING

==============================================================================
*/

void
ExpandCommand (char *in, char *out, char *src, char *dest)
{
	while (*in) {
		if (in[0] == '$') {
			if (in[1] == '1') {
				strcpy (out, src);
				out += strlen (src);
			} else if (in[1] == '2') {
				strcpy (out, dest);
				out += strlen (dest);
			}
			in += 2;
			continue;
		}
		*out++ = *in++;
	}
	*out = 0;
}


/*
=============
saveBSP
=============
*/
-saveBSP:(char *)
cmdline     dialog:(BOOL) wt
{
	char        expandedcmd[1024];
	char        mappath[1024];
	char        bsppath[1024];
	int         oldLightFilter;
	int         oldPathFilter;
	char       *destdir;

	if (bsppid) {
		NSBeep ();
		return self;
	}
//
// turn off the filters so all entities get saved
//
	oldLightFilter =[filter_light_i intValue];
	oldPathFilter =[filter_path_i intValue];
	[filter_light_i setIntValue:0];
	[filter_path_i setIntValue:0];
	[self applyRegion:self];

	if ([regionbutton_i intValue]) {
		strcpy (mappath, filename);
		// XXX StripExtension (mappath);
		strcat (mappath, ".reg");
		[map_i writeMapFile: mappath useRegion:YES];
		wt = YES;						// allways pop the dialog on region ops
	} else
		strcpy (mappath, filename);

// save the entire thing, just in case there is a problem
	[self save:self];

	[filter_light_i setIntValue:oldLightFilter];
	[filter_path_i setIntValue:oldPathFilter];
	[self applyRegion:self];

//
// write the command to the bsp host
//  
	destdir =[project_i getFinalMapDirectory];

	strcpy (bsppath, destdir);
	strcat (bsppath, "/");
	// XXX ExtractFileBase (mappath, bsppath + strlen(bsppath));
	strcat (bsppath, ".bsp");

	ExpandCommand (cmdline, expandedcmd, mappath, bsppath);

	strcat (expandedcmd, " > ");
	strcat (expandedcmd, FN_CMDOUT);
	strcat (expandedcmd, "\n");
	printf ("system: %s", expandedcmd);

	[project_i addToOutput:"\n\n========= BUSY =========\n\n"];
	[project_i addToOutput:expandedcmd];

	if ([preferences_i getShowBSP])
		[inspcontrol_i changeInspectorTo:i_output];

	if (wt) {
		id          panel;

		panel = NSGetAlertPanel (@"BSP In Progress",[NSString stringWithCString:expandedcmd], NULL, NULL, NULL);
		[panel makeKeyAndOrderFront:NULL];
		system (expandedcmd);
		[panel release];
		[self makeKeyAndOrderFront:NULL];
		DisplayCmdOutput ();
	} else {
		// cmdte = DPSAddTimedEntry(1, CheckCmdDone, self, NS_BASETHRESHOLD);
		if (!(bsppid = fork ())) {
			system (expandedcmd);
			exit (0);
		}
	}

	return self;
}


-BSP_Full:sender
{
	[self saveBSP: [project_i getFullVisCmd] dialog:NO];
	return self;
}

-BSP_FastVis:sender
{
	[self saveBSP: [project_i getFastVisCmd] dialog:NO];
	return self;
}

-BSP_NoVis:sender
{
	[self saveBSP: [project_i getNoVisCmd] dialog:NO];
	return self;
}

-BSP_relight:sender
{
	[self saveBSP: [project_i getRelightCmd] dialog:NO];
	return self;
}

-BSP_entities:sender
{
	[self saveBSP: [project_i getEntitiesCmd] dialog:NO];
	return self;
}

-BSP_stop:sender
{
	if (!bsppid) {
		NSBeep ();
		return self;
	}

	kill (bsppid, 9);
	// CheckCmdDone (cmdte, 0, NULL);
	[project_i addToOutput:"\n\n========= STOPPED =========\n\n"];

	return self;
}



/*
==============
doOpen:

Called by open or the project panel
==============
*/
-doOpen:(char *) fname;
{
	strcpy (filename, fname);

	[map_i readMapFile:filename];

	[regionbutton_i setIntValue:0];
	[self setTitleWithRepresentedFilename: [NSString stringWithCString:fname]];
	[self updateAll];

	Sys_Printf ("%s loaded\n", fname);

	return self;
}


/*
==============
open
==============
*/
-open:sender;
{
	id          openpanel;
	NSString   *suffixlist[] = { @"map" };

	openpanel =[NSOpenPanel new];

	if ([openpanel runModalForDirectory: [NSString stringWithCString:[project_i
	 getMapDirectory]]
	file: @"" types: [NSArray arrayWithObjects: suffixlist count:1]] !=
		NSOKButton)
		return self;

	[self doOpen:(char *)[[openpanel filename] cString]];

	return self;
}


/*
==============
save:
==============
*/
-save:sender;
{
	char        backup[1024];

// force a name change if using tempname
	if (!strcmp (filename, FN_TEMPSAVE))
		return[self saveAs:self];

	dirty = autodirty = NO;

	strcpy (backup, filename);
	// XXX StripExtension (backup);
	strcat (backup, ".bak");
	rename (filename, backup);			// copy old to .bak

	[map_i writeMapFile: filename useRegion:NO];

	return self;
}


/*
==============
saveAs
==============
*/
-saveAs:sender;
{
	id          panel_i;
	char        dir[1024];

	panel_i =[NSSavePanel new];
	// XXX ExtractFileBase (filename, dir);
	[panel_i setRequiredFileType:@"map"];
	if ([panel_i runModalForDirectory: [NSString stringWithCString: [project_i getMapDirectory]] file: [NSString stringWithCString:dir]] !=
		NSOKButton)
		return self;

	strcpy (filename,[[panel_i filename] cString]);

	[self setTitleWithRepresentedFilename: [NSString stringWithCString:filename]];

	[self save:self];

	return self;
}


/*
===============================================================================

						OTHER METHODS

===============================================================================
*/


//
//  AJR - added this for Project info
//
-(char *) currentFilename
{
	return filename;
}

-deselect:sender
{
	if ([clipper_i hide])				// first click hides only the clipper
		return[self updateAll];

	[map_i setCurrentEntity: [map_i objectAtIndex:0]];
										// make world selected
	[map_i makeSelectedPerform:@selector (deselect)];
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

-keyDown:(NSEvent *) theEvent
{
	int         ch;
	const char *chars;

// function keys
	switch ([theEvent keyCode]) {
		case 60:						// F2
			[cameraview_i setDrawMode:dr_wire];
			Sys_Printf ("wire draw mode\n");
			return self;
		case 61:						// F3
			[cameraview_i setDrawMode:dr_flat];
			Sys_Printf ("flat draw mode\n");
			return self;
		case 62:						// F4
			[cameraview_i setDrawMode:dr_texture];
			Sys_Printf ("texture draw mode\n");
			return self;

		case 63:						// F5
			[xyview_i setDrawMode:dr_wire];
			Sys_Printf ("wire draw mode\n");
			return self;
		case 64:						// F6
			Sys_Printf ("texture draw mode\n");
			return self;

		case 66:						// F8
			[cameraview_i homeView:self];
			return self;

		case 88:						// F12
			[map_i subtractSelection:self];
			return self;

		case 106:						// page up
			[cameraview_i upFloor:self];
			return self;

		case 107:						// page down
			[cameraview_i downFloor:self];
			return self;

		case 109:						// end
			[self deselect:self];
			return self;
	}

// portable things
	chars = [[theEvent characters] cString];
	ch = chars ? tolower (chars[0]) : 0;

	switch (ch) {
		case KEY_RIGHTARROW:
		case KEY_LEFTARROW:
		case KEY_UPARROW:
		case KEY_DOWNARROW:
		case 'a':
		case 'z':
		case 'd':
		case 'c':
		case '.':
		case ',':
			[cameraview_i _keyDown:theEvent];
			break;

		case 27:						// escape
			autodirty = dirty = YES;
			[self deselect:self];
			return self;

		case 127:						// delete
			autodirty = dirty = YES;
			[map_i makeSelectedPerform:@selector (remove)];
			[clipper_i hide];
			[self updateAll];
			break;

		case '/':
			[clipper_i flipNormal];
			[self updateAll];
			break;

		case 13:						// enter
			[clipper_i carve];
			[self updateAll];
			Sys_Printf ("carved brush\n");
			break;

		case ' ':
			[map_i cloneSelection:self];
			break;


//
// move selection keys
//      
		case '2':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[1] = -[xyview_i gridsize];
			[map_i makeSelectedPerform:@selector (translate)];
			[self updateAll];
			break;
		case '8':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[1] =[xyview_i gridsize];
			[map_i makeSelectedPerform:@selector (translate)];
			[self updateAll];
			break;

		case '4':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[0] = -[xyview_i gridsize];
			[map_i makeSelectedPerform:@selector (translate)];
			[self updateAll];
			break;
		case '6':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[0] =[xyview_i gridsize];
			[map_i makeSelectedPerform:@selector (translate)];
			[self updateAll];
			break;

		case '-':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[2] = -[xyview_i gridsize];
			[map_i makeSelectedPerform:@selector (translate)];
			[self updateAll];
			break;
		case '+':
			VectorCopy (vec3_origin, sb_translate);
			sb_translate[2] =[xyview_i gridsize];
			[map_i makeSelectedPerform:@selector (translate)];
			[self updateAll];
			break;

		default:
			Sys_Printf ("undefined keypress\n");
			NopSound ();
			break;
	}

	return self;
}


@end
