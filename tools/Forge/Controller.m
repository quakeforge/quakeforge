/*
	Controller.h

	Application controller class

	Copyright (C) 2001 Dusk to Dawn Computing, Inc.

	Author: Jeff Teunissen <deek@d2dc.net>
	Date:	5 Nov 2001

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

#import <AppKit/NSApplication.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSWindow.h>

#import "Controller.h"
#import "PrefsController.h"

@implementation Controller

- (BOOL) application: (NSApplication *) app openFile: (NSString *) filename;
{
	return NO;
}

- (BOOL) application: (NSApplication *) app openTempFile: (NSString *) filename;
{
	return NO;
}

- (BOOL) applicationOpenUntitledFile: (NSApplication *) app;
{
	return NO;
}

- (BOOL) applicationShouldOpenUntitledFile: (NSApplication *) app;
{
	return NO;
}

- (BOOL) applicationShouldTerminate: (NSApplication *) app;
{
	return YES;
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication *) app;
{
	return YES;
}

/******
	Action methods
******/

- (void) showPreferencesPanel: (id) sender;
{
	NSDebugLog (@"Showing Preferences panel...");
	[prefsController orderFrontPreferencesPanel: self];
}

/*
	Project management
*/
- (void) newProject: (id) sender;
{
	NSLog (@"This _will_ create a new project, but it doesn't yet.");
}

- (void) openProject: (id) sender;
{
	NSLog (@"This _will_ open a project, but it doesn't yet.");
}

- (void) saveProject: (id) sender;
{
	NSLog (@"This _will_ save the project, but it doesn't yet.");
}

- (void) closeProject: (id) sender;
{
	NSLog (@"This _will_ close the project, but it doesn't yet.");
}

/*
	File-level stuff.
*/
- (void) addFileToProject: (id) sender;
{
	NSLog (@"This _will_ copy/move a file into the project, but it doesn't yet.");
}

- (void) addNewFileToProject: (id) sender;
{
	NSLog (@"This _will_ create a new file, but it doesn't yet.");
}

- (void) open: (id) sender;
{
	NSLog (@"This _will_ open a file, but it doesn't yet.");
}

/******
	Notifications
******/

/*
	applicationDidFinishLaunching:

	Sent when the app has finished starting up
*/
- (void) applicationDidFinishLaunching: (NSNotification *) not;
{
	[bundleController loadBundles];
}

/*
	applicationWillFinishLaunching:

	Sent when the app is just about to complete its startup
*/
- (void) applicationWillFinishLaunching: (NSNotification *) not;
{
	NSMenu	*menu = [NSApp mainMenu];

	/*
		Windows
	*/
	NSDebugLog (@"Windows");
	[NSApp setWindowsMenu: [[menu itemWithTitle: _(@"Windows")] submenu]];

	/*
		Services
	*/
	NSDebugLog (@"Services");
	[NSApp setServicesMenu: [[menu itemWithTitle: _(@"Services")] submenu]];
}

/*
	applicationWillTerminate:

	We're about to die, but AppKit is giving us a chance to clean up
*/
- (void) applicationWillTerminate: (NSNotification *) not;
{
}

/******
	Nib awakening
******/
- (void) awakeFromNib
{
	[window setFrameAutosaveName: @"Project View"];
	[window setFrameUsingName: @"Project View"];
}

/******
	Bundle Controller delegate methods
******/

- (void) bundleController: (BundleController *) aController didLoadBundle: (NSBundle *) aBundle
{
	NSDictionary	*info = nil;

	if (!aBundle) {
		NSLog (@"Controller -bundleController: sent nil bundle");
		return;
	}

	info = [aBundle infoDictionary];
	if (![aBundle principalClass]) {
		
		if (!(info || [info objectForKey: @"NSExecutable"])) {
			NSLog (@"%@ has no principal class and no info dictionary", aBundle);
			return;
		}

		NSLog (@"Bundle `%@' has no principal class!", [[info objectForKey: @"NSExecutable"] lastPathComponent]);
		return;
	}
	if (![[aBundle principalClass] conformsToProtocol: @protocol(ForgeBundle)]) {
		NSLog (@"Bundle %@'s principal class does not conform to the ForgeBundle protocol.", [[info objectForKey: @"NSExecutable"] lastPathComponent]);
		return;
	}	
	[[(id <ForgeBundle>) [aBundle principalClass] alloc] initWithOwner: self];
}

- (PrefsController *) prefsController;
{
	return prefsController;
}

- (BOOL) registerPrefsController: (id <PrefsViewController>) aPrefsController
{
	if (!aPrefsController)
		return NO;

	[prefsController addPrefsViewController: aPrefsController];
	return YES;
}

@end
