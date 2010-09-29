#ifndef InspectorControl_h
#define InspectorControl_h

#include <AppKit/AppKit.h>

#define MINIWINICON "DoomEdIcon"

typedef enum {
	i_project,
	i_textures,
	i_things,
	i_prefs,
	i_settings,
	i_output,
	i_help,
	i_end
}  insp_e;

@class  InspectorControl;

extern InspectorControl  *inspcontrol_i;

@interface InspectorControl: NSObject
{
	IBOutlet NSView     *inspectorView_i;   // inspector view
	IBOutlet NSView     *inspectorSubview_i; // inspector view's current subview
	// (gets replaced)

	id  contentList;                // List of contentviews (corresponds to
	                                // insp_e enum order)

	id  windowList;                 // List of Windows (corresponds to
	                                // insp_e enum order)

	id  obj_textures_i;             // TexturePalette object (for
	                                // delegating)
	id  obj_genkeypair_i;           // GenKeyPair object

	NSPopUpButton   *popUpButton_i; // PopUpList title button
	NSMatrix        *popUpMatrix_i; // PopUpList matrix
	NSMutableArray  *itemList;      // List of popUp buttons

	IBOutlet NSTextView  *helpView;

	insp_e  currentInspectorType;   // keep track of current inspector

	//
	// Add id's here for new inspectors
	// **NOTE: Make sure PopUpList has correct TAG value that
	// corresponds to the enums above!

	// Windows
	IBOutlet NSWindow   *win_project_i; // project
	IBOutlet NSWindow   *win_textures_i; // textures
	IBOutlet NSWindow   *win_things_i;  // things
	IBOutlet NSWindow   *win_prefs_i;   // preferences
	IBOutlet NSWindow   *win_settings_i; // project settings
	IBOutlet NSWindow   *win_output_i;  // bsp output
	IBOutlet NSWindow   *win_help_i;    // documentation

	// PopUpList objs
	IBOutlet id <NSMenuItem>    itemProject_i;  // project
	IBOutlet id <NSMenuItem>    itemTextures_i; // textures
	IBOutlet id <NSMenuItem>    itemThings_i;   // things
	IBOutlet id <NSMenuItem>    itemPrefs_i;    // preferences
	IBOutlet id <NSMenuItem>    itemSettings_i; // project settings
	IBOutlet id <NSMenuItem>    itemOutput_i;   // bsp output
	IBOutlet id <NSMenuItem>    itemHelp_i; // docs
}

- (IBAction) changeInspector: (id)sender;

- (void) setCurrentInspector: (insp_e)which;
- (insp_e) currentInspector;

@end

@protocol InspectorControl
- (void) windowResized;
@end

#endif // InspectorControl_h
