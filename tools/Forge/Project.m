/*
	Project.m

	Project manager class

	Copyright (C) 2002 Dusk to Dawn Computing, Inc.

	Author: Jeff Teunissen <deek@d2dc.net>
	Date:	29 May 2002

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

#import <Foundation/NSFileManager.h>
#import <Foundation/NSString.h>

#import <AppKit/NSOpenPanel.h>
#import <AppKit/NSSavePanel.h>

#import "Controller.h"
#import "Project.h"

@implementation Project

- (id) init
{
	return self = [super init];
}

- (void) dealloc
{
	NSLog (@"Project deallocating");
	[super dealloc];
}

- (void) awakeFromNib
{
	int		result;

	if (![owner fileMode]) {
		NSLog (@"Project -awakeFromNib: Uhh, file mode isn't set. WTF?");
		[self dealloc];
		return;
	}

	switch ([owner fileMode]) {
		case COpenMode: {	// load project
			NSArray		*fileTypes = [NSArray arrayWithObject: @"forge"];
			NSOpenPanel	*oPanel = [NSOpenPanel openPanel];

			[oPanel setAllowsMultipleSelection: NO];
			[oPanel setCanChooseFiles: YES];
			[oPanel setCanChooseDirectories: NO];

			result = [oPanel runModalForTypes: fileTypes];

			if (result == NSOKButton) {		// got a path
				NSArray		*pathArray = [oPanel filenames];

				[self openProjectAtPath: [pathArray objectAtIndex: 0]];
			} else {
				[self dealloc];
			}
			break;
		}

		case CCreateMode: {	// Create new project
			NSSavePanel	*sPanel = [NSSavePanel savePanel];

			[sPanel setPrompt: _(@"Project location")];
			[sPanel setRequiredFileType: @"forge"];
			[sPanel setTreatsFilePackagesAsDirectories: NO];

			result = [sPanel runModal];

			if (result == NSOKButton) {		// got a path
				[self createProjectAtPath: [sPanel filename]];
			} else {
				[self dealloc];
			}
			break;
		}
	}
}

- (void) createProjectAtPath: (NSString *) aPath
{
	NSFileManager		*filer = [NSFileManager defaultManager];
	NSMutableDictionary	*dict = [[NSMutableDictionary alloc] init];

	if (!aPath) {
		NSLog (@"No path given!");
	}

	if (![filer createDirectoryAtPath: aPath attributes: nil]) {
		NSLog (@"Could not create directory %@", aPath);
	}

	// create dictionary with dictionaries in it, each containing nothing
	[dict writeToFile: [aPath stringByAppendingPathComponent: @"Forge.project"]
		   atomically: YES];
}

- (void) openProjectAtPath: (NSString *) aPath
{
	NSLog (@"No code to load project %@", aPath);
	[self dealloc];
}

- (void) closeProject: (id) sender
{
}

@end
