#ifndef Preferences_h
#define Preferences_h

#include <AppKit/AppKit.h>

extern id  preferences_i;

extern float  lightaxis[3];

// these are personal preferences saved in NeXT defaults, not project
// parameters saved in the quake.qe_project file

@interface Preferences: NSObject
{
	id  bspSound_i;                 // actual sound object

// internal state
	NSString *projectpath;
	NSString *bspSound;

	BOOL    brushOffset;
	BOOL    showBSP;

	float   xlight;
	float   ylight;
	float   zlight;                 // 0.0 - 1.0

	int  startwad;                  // 0 - 2

// UI targets
	id  startproject_i;             // TextField

	id  bspSoundField_i;            // TextField of bspSound

	id  brushOffset_i;              // Brush Offset checkbox
	id  showBSP_i;                  // Show BSP Output checkbox

	id  startwad_i;                 // which wad to load at startup

	id  xlight_i;                   // X-side lighting
	id  ylight_i;                   // Y-side lighting
	id  zlight_i;                   // Z-side lighting

	NSUserDefaults  *prefs;
}

- (id) readDefaults;

//
// validate and set methods called by UI or defaults
//
- (id) setProjectPath: (const char *)path;
- (id) setBspSoundPath: (NSString *)path; // set the path of the soundfile
- (id) setShowBSP: (int)state;      // set the state of ShowBSP
- (id) setBrushOffset: (int)state;  // set the state of BrushOffset
- (id) setStartWad: (int)value;     // set start wad (0-2)
- (id) setXlight: (float)value;     // set Xlight value for CameraView
- (id) setYlight: (float)value;     // set Ylight value for CameraView
- (id) setZlight: (float)value;     // set Zlight value for CameraView

//
// UI targets
//
- (id) setBspSound: sender;         // use OpenPanel to select sound
- (id) setCurrentProject: sender;   // make current project the default
- (id) UIChanged: sender;           // target for all checks and fields

//
// methods used by other objects to retreive defaults
//
- (id) playBspSound;

- (NSString *) getProjectPath;
- (int) getBrushOffset;             // get the state
- (int) getShowBSP;                 // get the state

- (float) getXlight;                // get Xlight value
- (float) getYlight;                // get Ylight value
- (float) getZlight;                // get Zlight value

- (int) getStartWad;

@end
#endif // Preferences_h
