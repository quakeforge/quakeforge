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

		directoryField = [[NSTextField alloc] initWithFrame: NSMakeRect (3, 174, 410, 24)];
		[directoryField setEditable: YES];
		[directoryField setSelectable: YES];
		[directoryField setAllowsEditingTextAttributes: NO];
		[directoryField setImportsGraphics: NO];
		[directoryField setTextColor: [NSColor blackColor]];
		[directoryField setBackgroundColor: [NSColor whiteColor]];
		[directoryField setBezeled: YES];
		[directoryField setTarget: owner];
		[directoryField setAction: @selector(projectPathChanged:)];
		[self addSubview: [directoryField autorelease]];

		findButton = [[NSButton alloc] initWithFrame: NSMakeRect (419, 174, 64, 24)];
		[findButton setTitle: @"Find..."];
		[findButton setTarget: owner];
		[findButton setAction: @selector(projectPathFindButtonClicked:)];
		[self addSubview: [findButton autorelease]];
	}
	return self;
}

- (id) directoryField
{
	return directoryField;
}

@end
