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

#ifdef HAVE_CONFIG_H
# include "Config.h"
#endif

#include <Foundation/NSFileManager.h>
#include <Foundation/NSString.h>
#include <Foundation/NSUserDefaults.h>

#include <AppKit/NSBrowser.h>
#include <AppKit/NSBrowserCell.h>
#include <AppKit/NSOpenPanel.h>
#include <AppKit/NSSavePanel.h>

#include "Controller.h"
#include "Project.h"

@implementation Project

- (id) init
{
	return self = [super init];
}

- (void) dealloc
{
	[rFiles release];
	[imgFiles release];
	[otherFiles release];
	[suppFiles release];
	NSLog (@"Project deallocating");
	[super dealloc];
}

- (void) awakeFromNib
{
	int				result;
	NSUserDefaults	*defaults = [NSUserDefaults standardUserDefaults];

	imgFiles = [NSMutableArray new];
	rFiles = [NSMutableArray new];
	suppFiles = [NSMutableArray new];
	otherFiles = [NSMutableArray new];

	if (![owner fileMode]) {
		NSLog (@"Project -awakeFromNib: Uhh, file mode isn't set. WTF?");
		[self dealloc];
		return;
	}

	if ([owner fileName]) {
		[self openProjectAtPath: [owner fileName]];
		return;
	}

	switch ([owner fileMode]) {
		case COpenMode: {	// load project
			NSArray		*fileTypes = [NSArray arrayWithObject: @"forge"];
			NSOpenPanel	*oPanel = [NSOpenPanel openPanel];

			[oPanel setAllowsMultipleSelection: NO];
			[oPanel setCanChooseFiles: YES];
			[oPanel setCanChooseDirectories: NO];

			result = [oPanel runModalForDirectory: [defaults objectForKey: @"ProjectPath"]
											 file: nil
											types: fileTypes];

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

			result = [sPanel runModalForDirectory: [defaults objectForKey: @"ProjectPath"]
											 file: @"ProjectName.forge"];

			if (result == NSOKButton) {		// got a path
				[self createProjectAtPath: [sPanel filename]];
			} else {
				[self dealloc];
			}
			break;
		}
		default:
			break;
	}
}

- (void) createProjectAtPath: (NSString *) aPath
{
	NSFileManager		*filer = [NSFileManager defaultManager];
	// create dictionary with dictionaries in it, each containing nothing
	NSMutableDictionary	*dict = [NSMutableDictionary dictionaryWithObjectsAndKeys:
								imgFiles, @"Image Files",
								rFiles, @"Ruamoko Source",
								suppFiles, @"Supporting Files",
								otherFiles, @"Unmanaged Files",
								nil];

	if (!aPath) {
		NSLog (@"No path given!");
	}

	if (![filer createDirectoryAtPath: aPath attributes: nil]) {
		NSLog (@"Could not create directory %@", aPath);
	}

	// write the dictionary
	[dict writeToFile: [aPath stringByAppendingPathComponent: @"Forge.project"]
		   atomically: YES];
	[window makeKeyAndOrderFront: self];
}

- (void) openProjectAtPath: (NSString *) aPath
{
	NSLog (@"No code to load project %@.", aPath);
	[self dealloc];
}

- (void) closeProject: (id) sender
{
}

- (int) browser: (NSBrowser *) sender numberOfRowsInColumn: (int) column
{
	return 1;
}

- (void) browser: (NSBrowser *) sender
 willDisplayCell: (id) cell
 		   atRow: (int) row
		  column: (int) column
{
	[cell setStringValue: @"hi"];
	[cell setLoaded: YES];
	[cell setLeaf: YES];
}
@end
