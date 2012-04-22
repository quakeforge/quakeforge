/*
	Controller.m

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

#ifdef HAVE_CONFIG_H
# include "Config.h"
#endif

#include <Foundation/NSDebug.h>

#include <AppKit/NSApplication.h>
#include <AppKit/NSNibLoading.h>
#include <AppKit/NSMenu.h>
#include <AppKit/NSWindow.h>

#include "Controller.h"
#include "PrefsController.h"

@implementation Controller

- (BOOL) application: (NSApplication *) app openFile: (NSString *) filename;
{
	if (![[filename pathExtension] isEqualToString: @"forge"]) {
		NSLog (@"File \"%@\" is not a project file!", filename);
		return NO;
	}

	fileMode = COpenMode;
	fileName = filename;

	if (![NSBundle loadNibNamed: @"Project" owner: self]) {
		NSLog (@"Could not load project manager for file \"%@\"", filename);
		fileMode = CNoMode;
		fileName = nil;
		return NO;
	}

	fileMode = CNoMode;
	fileName = nil;
	return YES;
}

- (BOOL) applicationShouldTerminate: (NSApplication *) app;
{
	return YES;
}

- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication *) app;
{
	return NO;
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
	fileMode = CCreateMode;

	if (![NSBundle loadNibNamed: @"Project" owner: self])
		NSLog (@"Could not create new Project window!");

	fileMode = CNoMode;
	return;
}

- (void) openProject: (id) sender;
{
	fileMode = COpenMode;

	if (![NSBundle loadNibNamed: @"Project" owner: self])
		NSLog (@"Could not create new Project window!");

	fileMode = CNoMode;
	return;
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

	[bundleController loadBundles];
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
	fileMode = CNoMode;
//	[window setFrameAutosaveName: @"Project View"];
//	[window setFrameUsingName: @"Project View"];
}

/******
	Bundle Controller delegate methods
******/

- (void) bundleController: (BundleController *) aController didLoadBundle: (NSBundle *) aBundle
{
	NSDictionary	*info = nil;

	/*
		Let's get paranoid about stuff we load... :)
	*/
	if (!aBundle) {
		NSLog (@"Controller -bundleController: sent nil bundle");
		return;
	}

	if (!(info = [aBundle infoDictionary])) {
		NSLog (@"Bundle %@ has no info dictionary!", aBundle);
		return;
	}

	if (![info objectForKey: @"NSExecutable"]) {
		NSLog (@"Bundle %@ has no executable!", aBundle);
		return;
	}

	if (![aBundle principalClass]) {
		NSLog (@"Bundle `%@' has no principal class!", [[info objectForKey: @"NSExecutable"] lastPathComponent]);
		return;
	}

	if (![[aBundle principalClass] conformsToProtocol: @protocol(ForgeBundle)]) {
		NSLog (@"Bundle %@'s principal class does not conform to the ForgeBundle protocol.", [[info objectForKey: @"NSExecutable"] lastPathComponent]);
		return;
	}

	[(id <ForgeBundle>) [[aBundle principalClass] alloc] initWithOwner: self];
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

- (CMode) fileMode
{
	return fileMode;
}

- (NSString *) fileName
{
	return fileName;
}

@end
