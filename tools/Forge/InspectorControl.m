
#import "qedefs.h"

// Add .h-files here for new inspectors
#import	"Things.h"
#import	"TexturePalette.h"
#import	"Preferences.h"

id		inspcontrol_i;

@implementation InspectorControl

- (id) awakeFromNib
{
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
	[self changeInspectorTo: i_project];

	return self;
}


//
//	Sent by the PopUpList in the Inspector
//	Each cell in the PopUpList must have the correct tag
//
- (void) changeInspector: (id) sender
{
	id	cell;

	cell = [sender selectedCell];
	[self changeInspectorTo: [cell tag]];
	return;
}

//
//	Change to specific Inspector
//
- (void) changeInspectorTo: (insp_e) which
{
	id		newView;
	NSRect	r;
	id		cell;
	NSRect	f;
	
	if (which == currentInspectorType)
		return;
	
	currentInspectorType = which;
	newView = [contentList objectAtIndex: which];
	
	cell = [itemList objectAtIndex: which];	// set PopUpButton title
	[popUpButton_i setTitle:[cell title]];
	
	[inspectorView_i replaceSubview:inspectorSubview_i with:newView];
	r = [inspectorView_i frame];
	inspectorSubview_i = newView;
	[inspectorSubview_i setAutoresizingMask: NSViewHeightSizable | NSViewWidthSizable];
	[inspectorSubview_i resizeWithOldSuperviewSize: r.size];
	
	[inspectorSubview_i lockFocus];
	r = [inspectorSubview_i bounds];

	PSsetgray (NSLightGray);
	NSRectFill (f);

	[inspectorSubview_i unlockFocus];
	[inspectorView_i display];
	
	return;
}

- (insp_e) currentInspector
{
	return currentInspectorType;
}


@end
