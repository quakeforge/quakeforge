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

#import <AppKit/AppKit.h>
#import "Controller.h"
#import "Preferences.h"

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

- (void) applicationDidFinishLaunching: (NSNotification *) not;
{
}

- (void) applicationWillFinishLaunching: (NSNotification *) not;
{
	NSMenu	*menu = [[[NSMenu alloc] init] autorelease];
	NSMenu	*info;
	NSMenu	*project;
	NSMenu	*edit;
	NSMenu	*bsp;
	NSMenu	*brush;
	NSMenu	*windows;
	NSMenu	*services;

	[menu addItemWithTitle: @"Info"		action: NULL	keyEquivalent: @""];
	[menu addItemWithTitle: @"Project"	action: NULL	keyEquivalent: @""];
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
					action: @selector (openFrontHelpPanel:)
			 keyEquivalent: @"?"];

	/*
		Project
	*/
	project = [[[NSMenu alloc] init] autorelease];
	[menu setSubmenu: project	forItem: [menu itemWithTitle: @"Project"]];

	[project addItemWithTitle: @"Open"
					   action: @selector (open:)
				keyEquivalent: @"o"];
	[project addItemWithTitle: @"Close"
					   action: @selector (closeProject:)
				keyEquivalent: @""];

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
	
	[menu setSubmenu: services	forItem: [menu itemWithTitle: @"Services"]];
	
	[NSApp setServicesMenu: services];

	[NSApp setMainMenu: menu];
}

- (void) applicationWillTerminate: (NSNotification *) not;
{
}
