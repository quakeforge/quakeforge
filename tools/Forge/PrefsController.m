/*
	PrefsController.m

	Preferences window controller class

	Copyright (C) 2001 Dusk to Dawn Computing, Inc.

	Author: Jeff Teunissen <deek@d2dc.net>
	Date:	11 Nov 2001

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

#import <Foundation/NSDebug.h>
#import <Foundation/NSUserDefaults.h>

#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>

#import "PrefsController.h"
#import "PrefsView.h"

@implementation PrefsController

static PrefsController	*sharedInstance = nil;
static NSMutableArray	*prefsViews = nil;

+ (PrefsController *) sharedPrefsController
{
	return (sharedInstance ? sharedInstance : [[self alloc] init]);
}

- (id) init
{
	if (sharedInstance) {
		[self dealloc];
	} else {
		self = [super init];
		sharedInstance = self;

		prefsViews = [[[NSMutableArray alloc] initWithCapacity: 5] retain];
	}
	return sharedInstance;	
}

- (void) awakeFromNib
{
	NSButtonCell	*prototype;

	// keep the window out of the menu until it's seen
	[window setExcludedFromWindowsMenu: YES];

	if (iconList)
		return;

	/* Prototype button for the matrix */
	prototype = [[[NSButtonCell alloc] init] autorelease];
	[prototype setButtonType: NSPushOnPushOffButton];
	[prototype setImagePosition: NSImageOverlaps];

	/* What is the matrix? */
	iconList = [[NSMatrix alloc] initWithFrame: NSMakeRect (0, 0, 64, 64)];
	[iconList setCellSize: NSMakeSize (64, 64)];
	[iconList setMode: NSRadioModeMatrix];
	[iconList setPrototype: prototype];
	[iconList setTarget: self];
	[iconList setAction: @selector(cellWasClicked:)];

	[scrollView setHasHorizontalScroller: YES];
	[scrollView setHasVerticalScroller: NO];
	[scrollView setDocumentView: iconList];
}

- (void) dealloc
{
	NSDebugLog (@"PrefsController -dealloc");

	[prefsViews release];
	[super dealloc];
}

- (void) windowWillClose: (NSNotification *) aNotification
{
}

- (void) orderFrontPreferencesPanel: (id) sender
{
	[window setExcludedFromWindowsMenu: NO];
	[window makeKeyAndOrderFront: self];
	return;
}

- (void) savePreferencesAndCloseWindow: (id) sender
{
	[self savePreferences: self];
	[window close];
}

- (void) savePreferences: (id) sender
{
	NSEnumerator				*enumerator = [prefsViews objectEnumerator];
	id <PrefsViewController>	current;

	NSDebugLog (@"Saving all preferences...");
	while ((current = [enumerator nextObject])) {
		[current savePrefs: self];
	}

	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) loadPreferences: (id) sender
{
	NSEnumerator				*enumerator = [prefsViews objectEnumerator];
	id <PrefsViewController>	current;

	NSDebugLog (@"Loading all preferences from database...");
	while ((current = [enumerator nextObject])) {
		[current loadPrefs: self];
	}

	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) resetToDefaults: (id) sender
{
	NSEnumerator				*enumerator = [prefsViews objectEnumerator];
	id <PrefsViewController>	current;

	NSDebugLog (@"Setting all preferences to default values...");
	while ((current = [enumerator nextObject])) {
		[current resetPrefsToDefault: self];
	}

	[[NSUserDefaults standardUserDefaults] synchronize];
}

- (void) addPrefsViewController: (id <PrefsViewController>) aController;
{
	NSButtonCell	*button = [[NSButtonCell alloc] init];

	if (! [prefsViews containsObject: aController]) {
		[prefsViews addObject: aController];
		[aController autorelease];
	}

	[button setTitle: [aController buttonCaption]];
	[button setFont: [NSFont systemFontOfSize: 9]];
	[button setImage: [aController buttonImage]];
	[button setImagePosition: NSImageAbove];
	[button setTarget: aController];
	[button setAction: [aController buttonAction]];

	[iconList addColumnWithCells: [NSArray arrayWithObject: button]];
	[iconList sizeToCells];
	[iconList setNeedsDisplay: YES];
//	[prefsPanel addPrefsViewButton: controller];
}

- (NSBox *) prefsViewBox
{
	return box;
}

@end
