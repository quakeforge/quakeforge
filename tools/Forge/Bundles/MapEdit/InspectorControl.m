#include "QF/sys.h"

#include "InspectorControl.h"

// Add .h-files here for new inspectors
#include "Things.h"
#include "TexturePalette.h"
#include "Preferences.h"

InspectorControl  *inspcontrol_i;

@interface CustomView: NSView
@end
@implementation CustomView
@end

@implementation InspectorControl

- (void) awakeFromNib
{
	NSBundle    *mainBundle = [NSBundle mainBundle];
	NSString    *path = [mainBundle  pathForResource: @"help" ofType: @"txt"
	                                     inDirectory: nil];
	NSString  *help = [NSString stringWithContentsOfFile: path];

	[helpView setString: help];

	inspcontrol_i = self;

	currentInspectorType = -1;

	contentList = [[NSMutableArray alloc] init];
	windowList = [[NSMutableArray alloc] init];
	itemList = [[NSMutableArray alloc] init];

	// ADD NEW INSPECTORS HERE...

	[windowList addObject: win_project_i];
	[contentList addObject: [win_project_i contentView]];
	[itemProject_i setKeyEquivalent: @"1"];
	[itemList addObject: itemProject_i];

	[windowList addObject: win_textures_i];
	[contentList addObject: [win_textures_i contentView]];
	[itemTextures_i setKeyEquivalent: @"2"];
	[itemList addObject: itemTextures_i];

	[windowList addObject: win_things_i];
	[contentList addObject: [win_things_i contentView]];
	[itemThings_i setKeyEquivalent: @"3"];
	[itemList addObject: itemThings_i];

	[windowList addObject: win_prefs_i];
	[contentList addObject: [win_prefs_i contentView]];
	[itemPrefs_i setKeyEquivalent: @"4"];
	[itemList addObject: itemPrefs_i];

	[windowList addObject: win_settings_i];
	[contentList addObject: [win_settings_i contentView]];
	[itemSettings_i setKeyEquivalent: @"5"];
	[itemList addObject: itemSettings_i];

	[windowList addObject: win_output_i];
	[contentList addObject: [win_output_i contentView]];
	[itemOutput_i setKeyEquivalent: @"6"];
	[itemList addObject: itemOutput_i];

	[windowList addObject: win_help_i];
	[contentList addObject: [win_help_i contentView]];
	[itemHelp_i setKeyEquivalent: @"7"];
	[itemList addObject: itemHelp_i];

	// Setup inspector window with project subview first

	[inspectorView_i setAutoresizesSubviews: YES];

	inspectorSubview_i = [contentList objectAtIndex: i_project];

	[inspectorView_i addSubview: inspectorSubview_i];

	currentInspectorType = -1;
	[self setCurrentInspector: i_project];
}

//
//  Sent by the PopUpList in the Inspector
//  Each cell in the PopUpList must have the correct tag
//
- (IBAction) changeInspector: sender
{
	[self setCurrentInspector: [sender selectedTag]];
}

//
//  Change to specific Inspector
//
- (void) setCurrentInspector: (insp_e)which
{
	id      newView;
	NSRect  r;
	NSRect  f;

	if (which == currentInspectorType)
		return;
	currentInspectorType = which;
	newView = [contentList objectAtIndex: which];

	[popUpButton_i selectItemAtIndex: which];

	[inspectorView_i replaceSubview: inspectorSubview_i with: newView];
	r = [inspectorView_i frame];
	inspectorSubview_i = newView;
	[inspectorSubview_i setAutoresizingMask: NSViewWidthSizable |
	 NSViewHeightSizable];
	r.size.width -= 4;
	r.size.height -= 4;
	[inspectorSubview_i setFrameSize: r.size];

	[inspectorSubview_i lockFocus];
	f = [inspectorSubview_i bounds];
	[[NSColor windowBackgroundColor] set];
	NSRectFill (f);
	[inspectorSubview_i unlockFocus];
	[inspectorView_i setNeedsDisplay: YES];
}

- (insp_e) currentInspector
{
	return currentInspectorType;
}

@end
