/*
	Preferences.h

	Preferences class definition for Forge

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

#import <Foundation/NSObject.h>
#import <Foundation/NSDictionary.h>

/*
	Keys in the dictionary
*/
#define ProjectPath	@"projectPath"
#define BspSoundPath @"bspSoundPath"
#define ShowBSPOutput @"showBSPOutput"
#define OffsetBrushCopy @"offsetBrushCopy"
#define StartWad @"startWad"
#define XLight @"xLight"
#define YLight @"yLight"
#define ZLight @"zLight"

@interface Preferences: NSObject
{
	// UI targets
	id		projectPathField;		// path to the project to load on startup
	id		bspSoundPathField;		// location of BSP sounds
	id		startWadField;			// which wadfile to load on startup
	id		xLightField;			// Lighting for X side
	id		yLightField;			// Lighting for Y side
	id		zLightField;			// Lighting for Z side
	id		showBSPOutputButton; 	// "Show BSP Output" checkbox
	id		offsetBrushCopyButton;	// "Brush offset" checkbox

	NSDictionary		*currentValues;
	NSMutableDictionary *displayedValues;
}

+ (void) saveDefaults;
- (void) loadDefaults;

+ (Preferences *) sharedInstance;	// Return the shared instance

- (NSDictionary *) preferences; 	// current prefs

- (void) updateUI;					// Update the displayed values in the UI
- (void) commitDisplayedValues; 	// Make displayed settings current
- (void) discardDisplayedValues;	// Replace displayed settings with current

// UI notifications
- (void) ok: (id) sender;				// commit displayed values
- (void) revert: (id) sender;			// revert to current values
- (void) revertToDefault: (id) sender;	// revert current values to defaults and
										// discard displayed values

- (void) prefsChanged: (id) sender;	// Notify the object to update the UI


- (id) objectForKey: (id) key;
//+ (void) setObject: (id) obj forKey: (id) key;


+ (NSDictionary *) preferencesFromDefaults;
+ (void) savePreferencesToDefaults: (NSDictionary *) dict;

@end

extern Preferences	*prefs;
