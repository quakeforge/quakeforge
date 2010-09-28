#ifndef QuakeEd_h
#define QuakeEd_h

#include <AppKit/AppKit.h>

extern id   quakeed_i;

extern BOOL filter_light, filter_path, filter_entities;
extern BOOL filter_clip_brushes, filter_water_brushes, filter_world;

extern id   g_cmd_out_i;

double      I_FloatTime (void);

void        NopSound (void);

@interface QuakeEd:NSWindow
{
	BOOL        dirty;
	char        filename[1024];			// full path with .map extension

	NSBitmapImageRep *cache[3];
	NSRect      cache_rect[3];
	BOOL		no_restore[3];

// UI objects
	id          brushcount_i;
	id          entitycount_i;
	id          regionbutton_i;

	id          show_coordinates_i;
	id          show_names_i;

	id          filter_light_i;
	id          filter_path_i;
	id          filter_entities_i;
	id          filter_clip_i;
	id          filter_water_i;
	id          filter_world_i;

	id          cmd_in_i;				// text fields
	id          cmd_out_i;

	id          xy_drawmode_i;			// passed over to xyview after init
}

-setDefaultFilename;
-(const char *) currentFilename;

-updateAll;								// when a model has been changed
-updateCamera;							// when the camera has moved
-updateXY;
-updateZ;

-updateAll:sender;

-(void)cameraNoRestore: (NSRect) rect;
-(void)xyNoRestore: (NSRect) rect;
-(void)zNoRestore: (NSRect) rect;

-newinstance;							// force next flushwindow to clear all
										// instance drawing
-redrawInstance;						// erase and redraw all instance now

-appWillTerminate:sender;

-openProject:sender;

-textCommand:sender;

-applyRegion:sender;

-(BOOL) dirty;

-clear:sender;
-centerCamera:sender;
-centerZChecker:sender;

-changeXYLookUp:sender;

-setBrushRegion:sender;
-setXYRegion:sender;

-open:sender;
-save:sender;
-saveAs:sender;

-doOpen:(const char *) fname;

-saveBSP:(const char *)cmdline dialog:(BOOL)wt;

-BSP_Full:sender;
-BSP_FastVis:sender;
-BSP_NoVis:sender;
-BSP_relight:sender;
-BSP_stop:sender;
-BSP_entities:sender;

-applicationDefined:(NSEvent *) theEvent;

//
// UI querie for other objects
//
-(BOOL) showCoordinates;
-(BOOL) showNames;

@end
#endif // QuakeEd_h
