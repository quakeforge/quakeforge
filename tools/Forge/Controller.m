/*
	Controller.m

	Central controller object for Edit...

	Copyright (c) 1995-1996, NeXT Software, Inc.
	All rights reserved.
	Author: Ali Ozer

	You may freely copy, distribute and reuse the code in this example.
	NeXT disclaims any warranty of any kind, expressed or implied,
	as to its fitness for any particular use.
*/

#import <AppKit/NSApplication.h>
#import <AppKit/NSMenu.h>
#import "Controller.h"

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
	NSMenu	*menu = [[[NSMenu alloc] init] autorelease];
	NSMenu	*info;
	NSMenu	*project;
	NSMenu	*file;
	NSMenu	*edit;
	NSMenu	*bsp;
	NSMenu	*brush;
	NSMenu	*windows;
	NSMenu	*services;

	[menu addItemWithTitle: @"Info"		action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: @"Project"	action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: @"File"		action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: @"Edit"		action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: @"BSP"		action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: @"Brush"	action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: @"Windows"	action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: @"Services"	action: NULL	keyEquivalent: @""];

	[menu addItemWithTitle: @"Hide"		action: @selector(hide:)	keyEquivalent: @"h"];
	[menu addItemWithTitle: @"Quit"		action: @selector(terminate:)	keyEquivalent: @"q"];

	/*
		Info
	*/
	info = [[[NSMenu alloc] init] autorelease];
	[menu setSubmenu: info	forItem: [menu itemWithTitle: @"Info"]];

	[info addItemWithTitle: @"Info Panel..."
					action: @selector (orderFrontStandardAboutPanel:)
			 keyEquivalent: @""];
	[info addItemWithTitle: @"Preferences..."
					action: @selector (orderFrontPreferencesPanel:)
			 keyEquivalent: @""];
	[info addItemWithTitle: @"Help"
					action: @selector (orderFrontHelpPanel:)
			 keyEquivalent: @"?"];

	/*
		Project
	*/
	project = [[[NSMenu alloc] init] autorelease];
	[menu setSubmenu: project	forItem: [menu itemWithTitle: @"Project"]];

	[project addItemWithTitle: @"Open..."
					   action: @selector (openProject:)
				keyEquivalent: @"@o"];
	[project addItemWithTitle: @"New"
					   action: @selector (createNewProject:)
				keyEquivalent: @"@n"];
	[project addItemWithTitle: @"Save..."
					   action: @selector (saveProject:)
				keyEquivalent: @"@s"];
	[project addItemWithTitle: @"Save As..."
					   action: @selector (saveProjectAs:)
				keyEquivalent: @"@S"];
	[project addItemWithTitle: @"Close"
					   action: @selector (closeProject:)
				keyEquivalent: @""];

	/*
		File
	*/
	file = [[[NSMenu alloc] init] autorelease];
	[menu setSubmenu: file	forItem: [menu itemWithTitle: @"File"]];

	[file addItemWithTitle: @"Open..."
					action: @selector (open:)
			 keyEquivalent: @"o"];
	[file addItemWithTitle: @"New"
					action: @selector (createNew:)
			 keyEquivalent: @"n"];
	[file addItemWithTitle: @"Save..."
					action: @selector (save:)
			 keyEquivalent: @"s"];
	[file addItemWithTitle: @"Save As..."
					action: @selector (saveAs:)
			 keyEquivalent: @"S"];
	[file addItemWithTitle: @"Save All"
					action: @selector (saveAll:)
			 keyEquivalent: @""];
	[file addItemWithTitle: @"Revert to Saved"
					action: @selector (revertToSaved:)
			 keyEquivalent: @""];
	[file addItemWithTitle: @"Close"
					action: @selector (close:)
			 keyEquivalent: @""];

	/*
		Edit
	*/
	edit = [[[NSMenu alloc] init] autorelease];
	[menu setSubmenu: edit forItem: [menu itemWithTitle: @"Edit"]];
	
	[edit addItemWithTitle: @"Undo"
					action: @selector (undo:)
			 keyEquivalent: @"z"];
	[edit addItemWithTitle: @"Redo"
					action: @selector (redo:)
			 keyEquivalent: @"Z"];
	[edit addItemWithTitle: @"Cut"
					action: @selector (cut:)
			 keyEquivalent: @"x"];
	[edit addItemWithTitle: @"Copy"
					action: @selector (copy:)
			 keyEquivalent: @"c"];
	[edit addItemWithTitle: @"Paste"
					action: @selector (paste:)
			 keyEquivalent: @"v"];

	/*
		Windows
	*/
	windows = [[[NSMenu alloc] init] autorelease];
	
	[menu setSubmenu: windows	forItem: [menu itemWithTitle: @"Windows"]];
	
	[NSApp setWindowsMenu: windows];

	/*
		Services
	*/
	services = [[[NSMenu alloc] init] autorelease];
	[NSApp setServicesMenu: services];
	[menu setSubmenu: services	forItem: [menu itemWithTitle: @"Services"]];

	[NSApp setMainMenu: menu];
}

/*
	applicationWillTerminate:

	Sort of like SIGQUIT. App should die now, but has a chance to clean up
*/
- (void) applicationWillTerminate: (NSNotification *) not;
{
}
