/*
	BundleController.m

	Bundle manager class

	Copyright (C) 2001 Dusk to Dawn Computing, Inc.
	Additional copyrights here

	Author: Jeff Teunissen <deek@d2dc.net>
	Date:	20 Nov 2001

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
#import <Foundation/NSFileManager.h>
#import <Foundation/NSPathUtilities.h>
#import <AppKit/NSPanel.h>

#import "BundleController.h"

@interface BundleController (Private)

- (void) loadBundleInPath: (NSString *) path;
- (NSArray *) bundlesWithExtension: (NSString *) extension inPath: (NSString *) path;

@end

@implementation BundleController (Private)

- (void) loadBundleInPath: (NSString *) path
{
	NSBundle	*bundle;

	if (!path) {
		NSLog (@"%@ -loadBundleInPath: No path given!", [[self class] description]);
		return;
	}

	NSDebugLog (@"Loading bundle %@...", path);
	
	if ((bundle = [NSBundle bundleWithPath: path])) {
		NSDebugLog (@"Bundle %@ successfully loaded.", path);

		/*
			Fire off the notification if we have a delegate that adopts the
			BundleDelegate protocol
		*/
		if (delegate && [delegate conformsToProtocol: @protocol(BundleDelegate)])
			[(id <BundleDelegate>) delegate bundleController: self didLoadBundle: bundle];
		[self bundleController: self didLoadBundle: bundle];
	} else {
		NSRunAlertPanel (@"Attention", @"Could not load bundle %@", @"OK", nil, nil, path);
	}
}

- (NSArray *) bundlesWithExtension: (NSString *) extension inPath: (NSString *) path
{
	NSMutableArray	*bundleList = [[NSMutableArray alloc] initWithCapacity: 10];
	NSEnumerator	*enumerator;
	NSFileManager	*fm = [NSFileManager defaultManager];
	NSString		*dir;
	BOOL			isDir;

	// ensure path exists, and is a directory
	if (![fm fileExistsAtPath: path isDirectory: &isDir])
		return nil;

	if (!isDir)
		return nil;

	// scan for bundles matching the extension in the dir
	enumerator = [[fm directoryContentsAtPath: path] objectEnumerator];
	while ((dir = [enumerator nextObject])) {
		if ([[dir pathExtension] isEqualToString: extension])
			[bundleList addObject: dir];
	}
	return bundleList;
}

@end

@implementation BundleController

- (id) init
{
	if ((self = [super init]))
		loadedBundles = [[NSMutableArray alloc] init];
	return self;
}

- (void) dealloc
{
	[loadedBundles release];
	
	[super dealloc];
}

- (id) delegate
{
	return delegate;
}

- (void) setDelegate: (id) aDelegate;
{
	delegate = aDelegate;
}

- (void) loadBundles
{
	NSMutableArray		*dirList = [[NSMutableArray alloc] initWithCapacity: 10];
	NSArray				*temp;
	NSMutableArray		*modified = [[NSMutableArray alloc] initWithCapacity: 10];
	NSEnumerator		*counter;
	id					obj;

	// Start out with our own resource dir
	[dirList addObject: [[NSBundle mainBundle] resourcePath]];

	// Get the library dirs and add our path to all of its entries
	temp = NSSearchPathForDirectoriesInDomains (NSLibraryDirectory, NSAllDomainsMask, YES);

	counter = [temp objectEnumerator];
	while ((obj = [counter nextObject])) {
		[modified addObject: [obj stringByAppendingPathComponent: @"Forge"]];
	}
	[dirList addObjectsFromArray: modified];

	// Okay, now go through dirList loading all of the bundles in each dir
	counter = [dirList objectEnumerator];
	while ((obj = [counter nextObject])) {
		NSEnumerator	*enum2 = [[self bundlesWithExtension: @"Forge" inPath: obj] objectEnumerator];
		NSString		*str;

		while ((str = [enum2 nextObject])) {
			[self loadBundleInPath: str];
		}
	}
}

- (NSArray *) loadedBundles
{
	return loadedBundles;
}

- (void) bundleController: (BundleController *) aController didLoadBundle: (NSBundle *) aBundle
{
	[loadedBundles addObject: aBundle];
}

@end
