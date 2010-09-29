#ifndef QuakeEd_h
#define QuakeEd_h

#include <AppKit/AppKit.h>

extern id  quakeed_i;

extern BOOL     filter_light, filter_path, filter_entities;
extern BOOL     filter_clip_brushes, filter_water_brushes, filter_world;

extern id  g_cmd_out_i;

double      I_FloatTime (void);

void        NopSound (void);

@interface QuakeEd: NSWindow
{
	BOOL    dirty;
	char    filename[1024];         // full path with .map extension

	NSBitmapImageRep    *cache[3];
	NSRect              cache_rect[3];
	BOOL                no_restore[3];

//
// UI objects
//
	id  brushcount_i;
	id  entitycount_i;
	id  regionbutton_i;

	id  show_coordinates_i;
	id  show_names_i;

	id  filter_light_i;
	id  filter_path_i;
	id  filter_entities_i;
	id  filter_clip_i;
	id  filter_water_i;
	id  filter_world_i;

	id  cmd_in_i;                   // text fields
	id  cmd_out_i;

	id  xy_drawmode_i;              // passed over to xyview after init
}

- (id) setDefaultFilename;
- (const char *) currentFilename;

- (id) updateAll;                   // when a model has been changed
- (id) updateCamera;                // when the camera has moved
- (id) updateXY;
- (id) updateZ;

- (id) updateAll: sender;

- (void) cameraNoRestore: (NSRect)rect;
- (void) xyNoRestore: (NSRect)rect;
- (void) zNoRestore: (NSRect)rect;

- (id) newinstance;                 // force next flushwindow to clear all
                                    // instance drawing
- (id) redrawInstance;              // erase and redraw all instance now

- (id) appWillTerminate: sender;

- (id) openProject: sender;

- (id) textCommand: sender;

- (id) applyRegion: sender;

- (BOOL) dirty;

- (id) clear: sender;
- (id) centerCamera: sender;
- (id) centerZChecker: sender;

- (id) changeXYLookUp: sender;

- (id) setBrushRegion: sender;
- (id) setXYRegion: sender;

- (id) open: sender;
- (id) save: sender;
- (id) saveAs: sender;

- (id) doOpen: (const char *)fname;

- (id) saveBSP: (const char *)cmdline dialog: (BOOL)wt;

- (id) BSP_Full: sender;
- (id) BSP_FastVis: sender;
- (id) BSP_NoVis: sender;
- (id) BSP_relight: sender;
- (id) BSP_stop: sender;
- (id) BSP_entities: sender;

- (id) applicationDefined: (NSEvent *)theEvent;

//
// UI query for other objects
//
- (BOOL) showCoordinates;
- (BOOL) showNames;

@end
#endif // QuakeEd_h
