
extern id	quakeed_i;

extern BOOL	filter_light, filter_path, filter_entities;
extern BOOL	filter_clip_brushes, filter_water_brushes, filter_world;

extern UserPath	*upath;

extern	id	g_cmd_out_i;

double I_FloatTime (void);

void NopSound (void);

void qprintf (char *fmt, ...);		// prints text to cmd_out_i

@interface Forge: NSWindow
{
	BOOL			dirty;
	NSMutableString	*filename;		// full path with .map extension
	unsigned int	eventMask;

	// UI objects
	id		brushcount_i;
	id		entitycount_i;
	id		regionbutton_i;

	id		show_coordinates_i;
	id		show_names_i;

	id		filter_light_i;
	id		filter_path_i;
	id		filter_entities_i;
	id		filter_clip_i;
	id		filter_water_i;
	id		filter_world_i;
	
	id		cmd_in_i;		// text fields
	id		cmd_out_i;	
	
	id		xy_drawmode_i;	// passed over to xyview after init
}

- (void) setDefaultFilename;
- (NSString *) currentFilename;

// events
- (void) postAppDefined;	// post an NSAppDefined event
- (void) addToEventMask: (unsigned int) mask;
- (unsigned int) eventMask;
- (NSEvent *) peekNextEvent;

- (void) updateAll;		// when a model has been changed
- (void) updateCamera;		// when the camera has moved
- (void) updateXY;
- (void) updateZ;

- (void) updateAll: (id) sender;

- (void) newinstance;		// force next flushwindow to clear all instance drawing
- (void) redrawInstance;	// erase and redraw all instance now

- (void) appWillTerminate: (id) sender;

- (void) openProject: (id) sender;

- (void) textCommand: (id) sender;

- (void) applyRegion: (id) sender;

- (BOOL) dirty;

- (void) clear: (id) sender;
- (void) centerCamera: (id) sender;
- (void) centerZChecker: (id) sender;

- (void) changeXYLookUp: (id) sender;

- (void) setBrushRegion: (id) sender;
- (void) setXYRegion: (id) sender;

- (void) open: (id) sender;
- (void) save: (id) sender;
- (void) saveAs: (id) sender;

- (void) doOpen: (NSString *) fname;

- (void) saveBSP: (NSString *) cmdline dialog: (BOOL) wt;

- (void) BSP_Full: (id) sender;
- (void) BSP_FastVis: (id) sender;
- (void) BSP_NoVis: (id) sender;
- (void) BSP_relight: (id) sender;
- (void) BSP_stop: (id) sender;
- (void) BSP_entities: (id) sender;

// UI queries for other objects
- (BOOL) showCoordinates;
- (BOOL) showNames;

@end

