/*
	MainPrefsController.m

	Controller class for this bundle

	Copyright (C) 2001 Dusk to Dawn Computing, Inc.
	Additional copyrights here

	Author: Jeff Teunissen <deek@d2dc.net>
	Date:	24 Nov 2001

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
*/
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "Config.h"
#endif

#import <AppKit/NSButton.h>
#ifdef USING_NIBS
# import <AppKit/NSNibLoading.h>
#endif

#import <AppKit/NSOpenPanel.h>

#import "PrefsPanel.h"
#import "PrefsController.h"
#import "MainPrefs.h"
#import "MainPrefsView.h"

@interface MainPrefs (Private)

- (NSDictionary *) preferencesFromDefaults;
- (void) savePreferencesToDefaults: (NSDictionary *) dict;

- (void) commitDisplayedValues;
- (void) discardDisplayedValues;

- (void) updateUI;

@end

@implementation MainPrefs (Private)

static NSDictionary			*currentValues = nil;
static NSMutableDictionary	*displayedValues = nil;

static NSMutableDictionary *
defaultValues (void) {
    static NSMutableDictionary *dict = nil;

    if (!dict) {
        dict = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
				@"/Local/Forge/Projects", ProjectPath,
				nil];
    }
    return dict;
}

static NSString *
getStringDefault (NSMutableDictionary *dict, NSString *name)
{
	NSString	*str = [[NSUserDefaults standardUserDefaults] stringForKey: name];

	if (!str)
		str = [defaultValues() objectForKey: name];

	[dict setObject: str forKey: name];
	
	return str;
}

- (NSDictionary *) preferencesFromDefaults
{
	NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity: 5];

	getStringDefault (dict, ProjectPath);
	return dict;
}

- (void) savePreferencesToDefaults: (NSDictionary *) dict
{
	NSUserDefaults	*defaults = [NSUserDefaults standardUserDefaults];

#define setStringDefault(name) \
	[defaults setObject: [dict objectForKey: (name)] forKey: (name)]

	NSDebugLog (@"Updating Main Preferences...");
	setStringDefault (ProjectPath);
	[defaults synchronize];
}

- (void) commitDisplayedValues
{
	[currentValues release];
	currentValues = [[displayedValues copy] retain];
	[self savePreferencesToDefaults: currentValues];
	[self updateUI];
}

- (void) discardDisplayedValues
{
	[displayedValues release];
	displayedValues = [[currentValues mutableCopy] retain];
	[self updateUI];
}

- (void) updateUI
{
	[projectPathField setStringValue: [displayedValues objectForKey: ProjectPath]];
	[projectPathField setNeedsDisplay: YES];
}

@end	// MainPrefs (Private)

@implementation MainPrefs

static MainPrefs			*sharedInstance = nil;
static id <BundleDelegate>	owner = nil;

- (id) initWithOwner: (id <BundleDelegate>) anOwner
{
	if (sharedInstance) {
		[self dealloc];
	} else {
		self = [super init];
		owner = anOwner;
		[owner registerPrefsController: self];
		if (![NSBundle loadNibNamed: @"MainPrefsView" owner: self]) {
			NSLog (@"MainPrefs: Could not load nib \"MainPrefsView\", using compiled version");
			view = [[MainPrefsView alloc] initWithOwner: self andFrame: PrefsRect];

			// hook up to our outlet(s)
			projectPathField = [view directoryField];
		} else {
			view = [window contentView];
		}
		[view retain];

		[self loadPrefs: self];

		sharedInstance = self;
	}
	return sharedInstance;
}

- (void) loadPrefs: (id) sender
{
	if (currentValues)
		[currentValues release];

	currentValues = [[[self preferencesFromDefaults] copyWithZone: [self zone]] retain];
	[self discardDisplayedValues];
}

- (void) savePrefs: (id) sender
{
	[self commitDisplayedValues];
}

- (void) resetPrefsToDefault: (id) sender
{
	if (currentValues)
		[currentValues release];

	currentValues = [[defaultValues () copyWithZone: [self zone]] retain];

	[self discardDisplayedValues];
}

- (void) showView: (id) sender;
{
	[[(PrefsPanel *)[[PrefsController sharedPrefsController] window] prefsViewBox] setContentView: view];
	[view setNeedsDisplay: YES];
}

- (NSView *) view
{
	return view;
}

- (NSString *) buttonCaption
{
	return @"Main";
}

- (NSImage *) buttonImage
{
	return [NSImage imageNamed: @"NSApplicationIcon"];
}

- (SEL) buttonAction
{
	return @selector(showView:);
}

- (id) projectPath
{
	return [displayedValues objectForKey: ProjectPath];
}

- (id) projectPathFindButtonClicked: (id) sender
{
	int			result;
	NSOpenPanel	*oPanel = [NSOpenPanel openPanel];

	[oPanel setAllowsMultipleSelection: NO];
	[oPanel setCanChooseFiles: NO];
	[oPanel setCanChooseDirectories: YES];

	result = [oPanel runModalForDirectory: NSHomeDirectory() file: nil types: nil];
	if (result == NSOKButton) {		// got a new dir
		NSArray		*pathArray = [oPanel filenames];

		[displayedValues setObject: [pathArray objectAtIndex: 0] forKey: ProjectPath];
		[self updateUI];
	}
}

- (void) projectPathChanged: (id) sender
{
	[displayedValues setObject: [sender stringValue] forKey: ProjectPath];
	[self updateUI];
}

@end
