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

#import <AppKit/NSApplication.h>
#import <AppKit/NSMenu.h>
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

/*
	Action methods
*/
- (void) createNew: (id) sender;
{
	NSLog (@"This _would_ create a new file, but it doesn't.");
}

- (void) createNewProject: (id) sender;
{
	NSLog (@"This _would_ create a new project, but it doesn't.");
}

- (void) infoPanel: (id) sender;
{
	[NSApp orderFrontStandardAboutPanel: self];
}

- (void) open: (id) sender;
{
	NSLog (@"This _would_ open a file, but it doesn't.");
}

- (void) openProject: (id) sender;
{
	NSLog (@"This _would_ open a project, but it doesn't.");
}

- (void) saveAll: (id) sender;
{
	NSLog (@"This _would_ save, but it doesn't.");
}

- (void) showPreferencesPanel: (id) sender;
{
	NSDebugLog (@"Showing Preferences panel...");
	[[PrefsController sharedPrefsController] orderFrontPreferencesPanel: self];
}

/*
	Notifications
*/

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
	NSMenu	*info;
	NSMenu	*project;
	NSMenu	*file;
	NSMenu	*edit;
	NSMenu	*windows;
	NSMenu	*services;

	[menu addItemWithTitle: _(@"Info")		action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: _(@"Project")	action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: _(@"File")		action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: _(@"Edit")		action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: _(@"Windows")	action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: _(@"Services")	action: NULL	keyEquivalent: @""];

	[menu addItemWithTitle: _(@"Hide")		action: @selector(hide:)	keyEquivalent: @"h"];
	[menu addItemWithTitle: _(@"Quit")		action: @selector(terminate:)	keyEquivalent: @"q"];

	/*
		Info
	*/
	NSDebugLog (@"Info");
	info = [[[NSMenu alloc] init] autorelease];
	[menu setSubmenu: info	forItem: [menu itemWithTitle: _(@"Info")]];

	[info addItemWithTitle: _(@"Info Panel...")
					action: @selector (orderFrontStandardAboutPanel:)
			 keyEquivalent: @""];
	[info addItemWithTitle: _(@"Preferences...")
					action: @selector (showPreferencesPanel:)
			 keyEquivalent: @""];
	[info addItemWithTitle: _(@"Help")
					action: @selector (orderFrontHelpPanel:)
			 keyEquivalent: @"?"];

	/*
		Project
	*/
	NSDebugLog (@"Project");
	project = [[[NSMenu alloc] init] autorelease];
	[menu setSubmenu: project	forItem: [menu itemWithTitle: _(@"Project")]];

	[project addItemWithTitle: _(@"Open...")
					   action: @selector (openProject:)
				keyEquivalent: @"o"];
	[project addItemWithTitle: _(@"New")
					   action: @selector (createNewProject:)
				keyEquivalent: @"n"];
	[project addItemWithTitle: _(@"Save...")
					   action: @selector (saveProject:)
				keyEquivalent: @"s"];
	[project addItemWithTitle: _(@"Save As...")
					   action: @selector (saveProjectAs:)
				keyEquivalent: @"S"];
	[project addItemWithTitle: _(@"Close")
					   action: @selector (closeProject:)
				keyEquivalent: @""];

	/*
		File
	*/
	NSDebugLog (@"File");
	file = [[[NSMenu alloc] init] autorelease];
	[menu setSubmenu: file	forItem: [menu itemWithTitle: _(@"File")]];

	[file addItemWithTitle: _(@"Add File...")
					action: @selector (addFileToProject:)
			 keyEquivalent: @"a"];
	[file addItemWithTitle: _(@"Add New File...")
					action: @selector (addNewFileToProject:)
			 keyEquivalent: @"N"];
	[file addItemWithTitle: _(@"Save File")
					action: @selector (saveFile:)
			 keyEquivalent: @"f"];
	[file addItemWithTitle: _(@"Revert to Saved")
					action: @selector (revertToSaved:)
			 keyEquivalent: @""];
	[file addItemWithTitle: _(@"Close")
					action: @selector (close:)
			 keyEquivalent: @""];

	/*
		Edit
	*/
	NSDebugLog (@"Edit");
	edit = [[[NSMenu alloc] init] autorelease];
	[menu setSubmenu: edit forItem: [menu itemWithTitle: _(@"Edit")]];
	
	[edit addItemWithTitle: _(@"Undo")
					action: @selector (undo:)
			 keyEquivalent: @"z"];
	[edit addItemWithTitle: _(@"Redo")
					action: @selector (redo:)
			 keyEquivalent: @"Z"];
	[edit addItemWithTitle: _(@"Cut")
					action: @selector (cut:)
			 keyEquivalent: @"x"];
	[edit addItemWithTitle: _(@"Copy")
					action: @selector (copy:)
			 keyEquivalent: @"c"];
	[edit addItemWithTitle: _(@"Paste")
					action: @selector (paste:)
			 keyEquivalent: @"v"];
	[edit addItemWithTitle: _(@"Delete")
					action: @selector (delete:)
			 keyEquivalent: @""];
	[edit addItemWithTitle: _(@"Select All")
					action: @selector (selectAll:)
			 keyEquivalent: @"a"];

	/*
		Windows
	*/
	NSDebugLog (@"Windows");
	windows = [[[NSMenu alloc] init] autorelease];

	[windows addItemWithTitle: _(@"Close Window")
					action: @selector (performClose:)
			 keyEquivalent: @"w"];
	[windows addItemWithTitle: _(@"Miniaturize Window")
					action: @selector (performMiniaturize:)
			 keyEquivalent: @"m"];
	[windows addItemWithTitle: _(@"Arrange in Front")
					action: @selector (arrangeInFront:)
			 keyEquivalent: @""];

	[NSApp setWindowsMenu: windows];

	/*
		Services
	*/
	NSDebugLog (@"Services");
	services = [[[NSMenu alloc] init] autorelease];

	[NSApp setServicesMenu: services];
	[menu setSubmenu: services	forItem: [menu itemWithTitle: _(@"Services")]];

	{	// yeah, yeah, shaddap
		id	controller = [[BundleController alloc] init];

		[controller setDelegate: self];
		[controller loadBundles];
	}
}

/*
	applicationWillTerminate:

	We're about to die, but AppKit is giving us a chance to clean up
*/
- (void) applicationWillTerminate: (NSNotification *) not;
{
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

- (BOOL) registerPrefsController: (id <PrefsViewController>) aPrefsController
{
	if (!aPrefsController)
		return NO;

	[[PrefsController sharedPrefsController] addPrefsViewController: aPrefsController];
	return YES;
}

@end
