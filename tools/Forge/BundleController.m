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

#ifdef HAVE_CONFIG_H
# include "Config.h"
#endif

#include <Foundation/NSDebug.h>
#include <Foundation/NSFileManager.h>
#include <Foundation/NSPathUtilities.h>
#include <Foundation/NSUserDefaults.h>

#include <AppKit/NSPanel.h>

#include "BundleController.h"

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
		[loadedBundles addObject: bundle];
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
			[bundleList addObject: [path stringByAppendingPathComponent: dir]];
	}
	return bundleList;
}

@end

@implementation BundleController

static BundleController *	sharedInstance = nil;

- (id) init
{
	if (sharedInstance) {
		[self dealloc];
	} else {
		self = [super init];
		loadedBundles = [[NSMutableArray alloc] init];
		sharedInstance = self;
	}
	return sharedInstance;
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
	unsigned int		domainMask = 0;
	NSUserDefaults		*defaults = [NSUserDefaults standardUserDefaults];
	id					obj;

	/*
		First, load and init all bundles in the app resource path
	*/
	NSDebugLog (@"Loading local bundles...");
	counter = [[self bundlesWithExtension: @"forgeb" inPath: [[NSBundle mainBundle] resourcePath]] objectEnumerator];
	while ((obj = [counter nextObject])) {
		[self loadBundleInPath: obj];
	}

	/*
		Then do the same for external bundles
	*/
	NSDebugLog (@"Loading foreign bundles...");
	// build domain mask, to find out where user wants to load bundles from
	if ([defaults boolForKey: @"BundlesFromUser"])
		domainMask |= NSUserDomainMask;
	if ([defaults boolForKey: @"BundlesFromLocal"])
		domainMask |= NSLocalDomainMask;
	if ([defaults boolForKey: @"BundlesFromNetwork"])
		domainMask |= NSNetworkDomainMask;
	if ([defaults boolForKey: @"BundlesFromSystem"])
		domainMask |= NSSystemDomainMask;

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
		NSEnumerator	*enum2 = [[self bundlesWithExtension: @"forgeb" inPath: obj] objectEnumerator];
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

@end
