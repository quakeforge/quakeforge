/*
	Preferences.m

	Preferences class

	Copyright (C) 2001 Dusk to Dawn Computing, Inc.

	Author: Jeff Teunissen <deek@d2dc.net>
	Date:	9 Feb 2001

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

#import <Foundation/NSDictionary.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSString.h>
#import <Foundation/NSUserDefaults.h>
#import <Foundation/NSValue.h>

#import <AppKit/NSButton.h>
#import <AppKit/NSControl.h>

#import "Preferences.h"

NSMutableDictionary *
defaultValues (void) {
    static NSMutableDictionary *dict = nil;

    if (!dict) {
        dict = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
				@"/Local/Forge/Projects", ProjectPath,
				@"/Local/Library/Sounds", BspSoundPath,
				@"none", StartWad,
				[NSNumber numberWithFloat: 1.0], XLight,
				[NSNumber numberWithFloat: 0.6], YLight,
				[NSNumber numberWithFloat: 0.75], ZLight,
				[NSNumber numberWithBool: NO], ShowBSPOutput,
				[NSNumber numberWithBool: NO], OffsetBrushCopy,
				nil];
    }
    return dict;
}

/***
	Code to deal with defaults
***/
BOOL
getBoolDefault (NSMutableDictionary *dict, NSString *name)
{
	NSString	*str = [[NSUserDefaults standardUserDefaults] stringForKey: name];
	NSNumber	*num;

	if (!str)
		str = [[defaultValues() objectForKey: name] stringValue];

	num = [NSNumber numberWithBool: [str hasPrefix: @"Y"]];
	[dict setObject: num forKey: name];

	return [num boolValue];
}

float
getFloatDefault (NSMutableDictionary *dict, NSString *name)
{
	NSString	*str = [[NSUserDefaults standardUserDefaults] stringForKey: name];
	NSNumber	*num;

	if (!str)
		str = [[defaultValues() objectForKey: name] stringValue];

	num = [NSNumber numberWithFloat: [str floatValue]];
	[dict setObject: num forKey: name];
	
	return [num floatValue];
}

int
getIntDefault (NSMutableDictionary *dict, NSString *name)
{
	NSString	*str = [[NSUserDefaults standardUserDefaults] stringForKey: name];
	NSNumber	*num;

	if (!str)
		str = [[defaultValues() objectForKey: name] stringValue];

	num = [NSNumber numberWithInt: [str intValue]];
	[dict setObject: num forKey: name];
	
	return [num intValue];
}

NSString *
getStringDefault (NSMutableDictionary *dict, NSString *name)
{
	NSString	*str = [[NSUserDefaults standardUserDefaults] stringForKey: name];

	if (!str)
		str = [defaultValues() objectForKey: name];

	[dict setObject: str forKey: name];
	
	return str;
}

@implementation Preferences

static Preferences			*sharedInstance = nil;
static NSDictionary			*currentValues = nil;
static NSMutableDictionary	*displayedValues = nil;

- (id) objectForKey: (id) key
{
	id		obj = [[NSUserDefaults standardUserDefaults] objectForKey: key];
	
	if (!obj)
		return [defaultValues () objectForKey: key];
	else
		return obj;
}

- (void) setObject: (id) obj forKey: (id) key
{
	if ((!key) || (!obj))
		return;

	[displayedValues setObject: obj forKey: key];
	[self commitDisplayedValues];
}

- (void) saveDefaults
{
	[self savePreferencesToDefaults: currentValues];
}

- (void) loadDefaults
{
	if (currentValues)
		[currentValues release];

	currentValues = [[self preferencesFromDefaults] copyWithZone: [self zone]];
	[self discardDisplayedValues];
}

+ (Preferences *) sharedInstance
{
	return (sharedInstance ? sharedInstance : [[self alloc] init]);
}

- (id) init
{
	if (sharedInstance) {
		[self dealloc];
	} else {
		[super init];
		currentValues = [[self preferencesFromDefaults] mutableCopy];
		[self discardDisplayedValues];
		sharedInstance = self;
		[[NSNotificationCenter defaultCenter]
			addObserver: self
			selector: @selector(saveDefaults)
			name: @"NSApplicationWillTerminateNotification"
			object: nil];
	}
	[self prefsChanged: self];
	return sharedInstance;
}

- (NSDictionary *) preferences
{
	return currentValues;
}

- (void) dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver: self];
	[currentValues release];
	[displayedValues release];
	currentValues = displayedValues = nil;
	[super dealloc];
}

/*
	updateUI

	Update the user interface with new preferences
*/
- (void) updateUI
{
	return;
}

- (void) prefsChanged: (id) sender
{
	float				aFloat;

	[displayedValues setObject: projectPath forKey: ProjectPath];
	[displayedValues setObject: bspSoundPath forKey: BspSoundPath];
	[displayedValues setObject: startWad forKey: StartWad];

	if ((aFloat = [[displayedValues objectForKey: XLight] floatValue]) < 0.0 || aFloat > 1.0) {
		aFloat = [[defaultValues () objectForKey: XLight] floatValue];
	}
	xLight = aFloat;
	[displayedValues setObject: [NSNumber numberWithFloat: xLight] forKey: XLight];

	if ((aFloat = [[displayedValues objectForKey: YLight] floatValue]) < 0.0 || aFloat > 1.0) {
		aFloat = [[defaultValues () objectForKey: YLight] floatValue];
	}
	yLight = aFloat;
	[displayedValues setObject: [NSNumber numberWithFloat: yLight] forKey: YLight];

	if ((aFloat = [[displayedValues objectForKey: ZLight] floatValue]) < 0.0 || aFloat > 1.0) {
		aFloat = [[defaultValues () objectForKey: ZLight] floatValue];
	}
	zLight = aFloat;
	[displayedValues setObject: [NSNumber numberWithFloat: zLight] forKey: ZLight];

	[displayedValues setObject: [NSNumber numberWithBool: showBSPOutput] forKey: ShowBSPOutput];
	[displayedValues setObject: [NSNumber numberWithBool: offsetBrushCopy] forKey: OffsetBrushCopy];

	[self commitDisplayedValues];
}

- (void) commitDisplayedValues
{
	[currentValues release];
	currentValues = [[displayedValues copy] retain];
	[self saveDefaults];
	[self updateUI];
}

- (void) discardDisplayedValues
{
	[displayedValues release];
	displayedValues = [[currentValues mutableCopy] retain];
	[self updateUI];
}

- (void) ok: (id) sender
{
	[self commitDisplayedValues];
}

- (void) revert: (id) sender
{
	[self discardDisplayedValues];
}

- (void) revertToDefault: (id) sender
{
	if (currentValues)
		[currentValues release];

	currentValues = defaultValues ();

	[self discardDisplayedValues];
}

- (NSDictionary *) preferencesFromDefaults
{
	NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity: 10];

	projectPath = getStringDefault (dict, ProjectPath);
	bspSoundPath = getStringDefault (dict, BspSoundPath);
	startWad = getStringDefault (dict, StartWad);
	xLight = getFloatDefault (dict, XLight);
	yLight = getFloatDefault (dict, YLight);
	zLight = getFloatDefault (dict, ZLight);
	showBSPOutput = getBoolDefault (dict, ShowBSPOutput);
	offsetBrushCopy = getBoolDefault (dict, OffsetBrushCopy);

	return dict;
}

#define setBoolDefault(name) \
{ \
	[defaults setBool:[[dict objectForKey:name] boolValue] forKey: name]; \
}

#define setFloatDefault(name) \
{ \
	[defaults setFloat:[[dict objectForKey:name] floatValue] forKey: name]; \
}

#define setIntDefault(name) \
{ \
	[defaults setInteger:[[dict objectForKey:name] intValue] forKey:name]; \
}

#define setStringDefault(name) \
{ \
	[defaults setObject: [dict objectForKey: name] forKey: name]; \
}

- (void) savePreferencesToDefaults: (NSDictionary *) dict
{
	NSUserDefaults	*defaults = [NSUserDefaults standardUserDefaults];

	NSLog (@"Updating defaults...");
	setStringDefault(ProjectPath);
	setStringDefault(BspSoundPath);
	setStringDefault(StartWad);
	setFloatDefault(XLight);
	setFloatDefault(YLight);
	setFloatDefault(ZLight);
	setBoolDefault(ShowBSPOutput);
	setBoolDefault(OffsetBrushCopy);
	[defaults synchronize];
}

@end
