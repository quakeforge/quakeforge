/*
	InspectorControl.h

	Inspector control class definitions

	Copyright (C) 2001 Jeff Teunissen <deek@d2dc.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/
#ifdef HAVE_CONFIG_H
# include "Config.h"
#endif

#import <AppKit/AppKit.h>

#define MINIWINICON	"DoomEdIcon"

typedef enum
{
	i_project,
	i_textures,
	i_things,
	i_prefs,
	i_settings,
	i_output,
	i_help,
	i_end
} insp_e;

extern	id		inspcontrol_i;

@interface InspectorControl:Object
{
	id	inspectorView_i;	// inspector view
	id	inspectorSubview_i;	// inspector view's current subview (gets replaced)

	id	contentList;		// List of contentviews (corresponds to
							// insp_e enum order)
	id	windowList;			// List of Windows (corresponds to
							// insp_e enum order)

	id	obj_textures_i;		// TexturePalette object (for delegating)
	id	obj_genkeypair_i;	// GenKeyPair object

	id	popUpButton_i;		// PopUpList title button
	id	popUpMatrix_i;		// PopUpList matrix
	id	itemList;			// List of popUp buttons
		
	insp_e	currentInspectorType;	// keep track of current inspector
	//
	//	Add id's here for new inspectors
	//  **NOTE: Make sure PopUpList has correct TAG value that
	//  corresponds to the enums above!
	
	// Windows
	id	win_project_i;		// project
	id	win_textures_i;		// textures
	id	win_things_i;		// things
	id	win_prefs_i;		// preferences
	id	win_settings_i;		// project settings
	id	win_output_i;		// bsp output
	id	win_help_i;			// documentation
	
	// PopUpList objs
	id	itemProject_i;		// project
	id	itemTextures_i;		// textures
	id	itemThings_i;		// things
	id	itemPrefs_i;		// preferences
	id	itemSettings_i;		// project settings
	id	itemOutput_i;		// bsp output
	id	itemHelp_i;			// docs
}

- (id) awakeFromNib;
- (void) changeInspector: (id) sender;
- (void) changeInspectorTo: (insp_e) which;
- (insp_e) currentInspector;

@end

@protocol InspectorControl
- windowResized;
@end
