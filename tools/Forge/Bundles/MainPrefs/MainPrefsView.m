/*
	MainPrefsView.m

	Forge internal preferences view

	Copyright (C) 2001 Dusk to Dawn Computing, Inc.

	Author: Jeff Teunissen <deek@d2dc.net>
	Date:	17 Nov 2001

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

#import <AppKit/NSBezierPath.h>
#import <AppKit/NSButton.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSColor.h>

#import "MainPrefsView.h"
#import "MainPrefs.h"

@implementation MainPrefsView
/*
	This class sucks, and shouldn't be necessary. With working "nibs", it isn't.
*/
- (id) initWithOwner: (id) anOwner andFrame: (NSRect) frameRect
{
	id		label = nil;

	owner = anOwner;

	if ((self = [super initWithFrame: frameRect])) {

		label = [[NSTextField alloc] initWithFrame: NSMakeRect (3, 201, 480, 24)];
		[label setEditable: NO];
		[label setSelectable: NO];
		[label setAllowsEditingTextAttributes: NO];
		[label setImportsGraphics: NO];
		[label setTextColor: [NSColor blackColor]];
		[label setBackgroundColor: [NSColor controlColor]];
		[label setBezeled: NO];
		[label setStringValue: @"Project load path"];
		[self addSubview: [label autorelease]];

		projectPathField = [[NSTextField alloc] initWithFrame: NSMakeRect (3, 174, 409, 24)];
		[projectPathField setEditable: YES];
		[projectPathField setSelectable: YES];
		[projectPathField setAllowsEditingTextAttributes: NO];
		[projectPathField setImportsGraphics: NO];
		[projectPathField setTextColor: [NSColor blackColor]];
		[projectPathField setBackgroundColor: [NSColor whiteColor]];
		[projectPathField setBezeled: YES];
		[projectPathField setTarget: owner];
		[projectPathField setAction: @selector(projectPathChanged:)];
		[self addSubview: [projectPathField autorelease]];

		projectPathFindButton = [[NSButton alloc] initWithFrame: NSMakeRect (419, 174, 64, 24)];
		[projectPathFindButton setTitle: @"Find..."];
		[projectPathFindButton setButtonType: NSMomentaryPushButton];
		[projectPathFindButton setTarget: owner];
		[projectPathFindButton setAction: @selector(projectPathFindButtonClicked:)];
		[self addSubview: [projectPathFindButton autorelease]];

		label = [[NSTextField alloc] initWithFrame: NSMakeRect (3, 85, 130, 20)];
		[label setEditable: NO];
		[label setSelectable: NO];
		[label setAllowsEditingTextAttributes: NO];
		[label setImportsGraphics: NO];
		[label setTextColor: [NSColor blackColor]];
		[label setBackgroundColor: [NSColor controlColor]];
		[label setBezeled: NO];
		[label setStringValue: @"Load bundles from:"];
		[self addSubview: [label autorelease]];

		bundlesFromUserButton = [[NSButton alloc] initWithFrame: NSMakeRect (160, 116, 150, 20)];
		[bundlesFromUserButton setTitle: @"Personal Library path"];
		[bundlesFromUserButton setButtonType: NSSwitchButton];
		[bundlesFromUserButton setImagePosition: NSImageRight];
		[bundlesFromUserButton setTarget: owner];
		[bundlesFromUserButton setAction: @selector(bundlesFromUserButtonChanged:)];
		[self addSubview: [bundlesFromUserButton autorelease]];

		bundlesFromLocalButton = [[NSButton alloc] initWithFrame: NSMakeRect (160, 93, 150, 20)];
		[bundlesFromLocalButton setTitle: @"Local Library path"];
		[bundlesFromLocalButton setButtonType: NSSwitchButton];
		[bundlesFromLocalButton setImagePosition: NSImageRight];
		[bundlesFromLocalButton setTarget: owner];
		[bundlesFromLocalButton setAction: @selector(bundlesFromLocalButtonChanged:)];
		[self addSubview: [bundlesFromLocalButton autorelease]];

		bundlesFromNetworkButton = [[NSButton alloc] initWithFrame: NSMakeRect (160, 70, 150, 20)];
		[bundlesFromNetworkButton setTitle: @"Networked Library path"];
		[bundlesFromNetworkButton setButtonType: NSSwitchButton];
		[bundlesFromNetworkButton setImagePosition: NSImageRight];
		[bundlesFromNetworkButton setTarget: owner];
		[bundlesFromNetworkButton setAction: @selector(bundlesFromNetworkButtonChanged:)];
		[self addSubview: [bundlesFromNetworkButton autorelease]];

		bundlesFromSystemButton = [[NSButton alloc] initWithFrame: NSMakeRect (160, 47, 150, 20)];
		[bundlesFromSystemButton setTitle: @"System Library path"];
		[bundlesFromSystemButton setButtonType: NSSwitchButton];
		[bundlesFromSystemButton setImagePosition: NSImageRight];
		[bundlesFromSystemButton setTarget: owner];
		[bundlesFromSystemButton setAction: @selector(bundlesFromSystemButtonChanged:)];
		[self addSubview: [bundlesFromSystemButton autorelease]];

	}
	return self;
}

- (id) projectPathField
{
	return projectPathField;
}

- (id) bundlesFromUserButton
{
	return bundlesFromUserButton;
}

- (id) bundlesFromLocalButton
{
	return bundlesFromLocalButton;
}

- (id) bundlesFromNetworkButton
{
	return bundlesFromNetworkButton;
}

- (id) bundlesFromSystemButton
{
	return bundlesFromSystemButton;
}

@end
