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

#import <AppKit/NSApplication.h>

#import "PrefsController.h"
#import "PrefsPanel.h"
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
	PrefsPanel		*prefsPanel;

	if (sharedInstance) {
		[self dealloc];
	} else {
		[super init];
		sharedInstance = self;

		prefsViews = [[[NSMutableArray alloc] initWithCapacity: 5] retain];

		prefsPanel = [[PrefsPanel alloc]
						initWithContentRect: NSMakeRect (250, 250, 516, 386)
						styleMask: NSTitledWindowMask
								 | NSMiniaturizableWindowMask
								 | NSClosableWindowMask
						backing: NSBackingStoreRetained
						defer: NO
					  ];
		[prefsPanel setTitle: [NSString stringWithFormat: @"%@ %@", _(@"Forge"), _(@"Preferences")]];

		[super initWithWindow: prefsPanel];
		[prefsPanel initUI];
		[prefsPanel setDelegate: self];
		[prefsPanel release];
	}
	return sharedInstance;	
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
	[[self window] makeKeyAndOrderFront: self];
	return;
}

- (void) savePreferencesAndCloseWindow: (id) sender
{
	[self savePreferences: self];
	[[self window] close];
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

- (void) addPrefsViewController: (id <PrefsViewController>) controller;
{
	PrefsPanel		*prefsPanel;

	prefsPanel = (PrefsPanel *) [self window];

	if (! [prefsViews containsObject: controller]) {
		[prefsViews addObject: controller];
		[controller autorelease];
	}

	[[prefsPanel prefsViewBox] setContentView: [controller view]];
	[[prefsPanel prefsViewBox] setNeedsDisplay: YES];
}

@end
