/*
	Preferences.m

	Preferences class for Forge

	Copyright (C) 2001 Jeff Teunissen <deek@quakeforge.net>

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

	$Id$
*/

#import <Foundation/NSNotification.h>
#import <Foundation/NSString.h>
#import <Foundation/NSUserDefaults.h>
#import <Foundation/NSValue.h>

#import <AppKit/NSButton.h>
#import <AppKit/NSControl.h>

#import "Preferences.h"

id	prefs;

static NSDictionary *defaultValues (void) {
    static NSDictionary *dict = nil;
    if (!dict) {
        dict = [[NSDictionary alloc] initWithObjectsAndKeys:
				@"/Local/Forge/Projects", ProjectPath,
				@"/Local/Forge/Sounds", BspSoundPath,
				[NSNumber numberWithInt: 0], StartWad,
				[NSNumber numberWithFloat: 1.0], XLight,
				[NSNumber numberWithFloat: 0.6], YLight,
				[NSNumber numberWithFloat: 0.75], ZLight,
				[NSNumber numberWithBool: NO], ShowBSPOutput,
				[NSNumber numberWithBool: NO], OffsetBrushCopy,
				nil];
    }
    return dict;
}

@implementation Preferences

static Preferences	*sharedInstance = nil;

- (id) objectForKey: (id) key
{
    return [[[[self class] sharedInstance] preferences] objectForKey: key];
}

+ (void) saveDefaults
{
	if (sharedInstance) {
		[self savePreferencesToDefaults: [sharedInstance preferences]];
	}
}

- (void) saveDefaults: (id) sender
{
	[[self class] saveDefaults];
}

- (void) loadDefaults
{
	if (currentValues)
		[currentValues release];

	currentValues = [[[self class] preferencesFromDefaults] copyWithZone: [self zone]];
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
		currentValues = [[[self class] preferencesFromDefaults] copyWithZone:[self zone]];
		[self discardDisplayedValues];
		sharedInstance = self;
		prefs = sharedInstance;
		[[NSNotificationCenter defaultCenter]
			addObserver: self
			selector: @selector(saveDefaults:)
			name: @"NSApplicationWillTerminateNotification"
			object: nil];
	}
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
	id theCenter = [NSNotificationCenter defaultCenter];

	NSLog (@"Defaults updated, UI should update.");

	[theCenter postNotificationName: @"ForgeTextureCacheShouldFlush" object: self userInfo: nil];
	[theCenter postNotificationName: @"ForgeUIShouldUpdate" object: self userInfo: nil];

	return;
}

- (void) prefsChanged: (id) sender {
	static NSNumber 	*yes = nil;
	static NSNumber 	*no = nil;
	int 				anInt;
	float				aFloat;
	
	if (!yes) {
		yes = [[NSNumber alloc] initWithBool: YES];
		no = [[NSNumber alloc] initWithBool: NO];
	}

	[displayedValues setObject: [projectPathField stringValue] forKey: ProjectPath];
	[displayedValues setObject: [bspSoundPathField stringValue] forKey: BspSoundPath];

	if ((anInt = [startWadField intValue]) < 0 || anInt > 2) {
		if ((anInt = [[displayedValues objectForKey: StartWad] intValue]) < 0 || anInt > 2) {
			anInt = [[defaultValues () objectForKey: StartWad] intValue];
		}
		[startWadField setIntValue: anInt];
	} else {
		[displayedValues setObject: [NSNumber numberWithInt: anInt] forKey: StartWad];
	}

	if ((aFloat = [xLightField floatValue]) < 0.0 || aFloat > 1.0) {
		if ((aFloat = [[displayedValues objectForKey: XLight] floatValue]) < 0.0 || aFloat > 1.0) {
			aFloat = [[defaultValues () objectForKey: XLight] floatValue];
		}
		[xLightField setFloatValue: aFloat];
	} else {
		[displayedValues setObject: [NSNumber numberWithFloat: aFloat] forKey: XLight];
	}

	if ((aFloat = [yLightField floatValue]) < 0.0 || aFloat > 1.0) {
		if ((aFloat = [[displayedValues objectForKey: YLight] floatValue]) < 0.0 || aFloat > 1.0) {
			aFloat = [[defaultValues () objectForKey: YLight] floatValue];
		}
		[yLightField setFloatValue: aFloat];
	} else {
		[displayedValues setObject: [NSNumber numberWithFloat: aFloat] forKey: YLight];
	}

	if ((aFloat = [zLightField floatValue]) < 0.0 || aFloat > 1.0) {
		if ((aFloat = [[displayedValues objectForKey: YLight] floatValue]) < 0.0 || aFloat > 1.0) {
			aFloat = [[defaultValues () objectForKey: YLight] floatValue];
		}
		[zLightField setFloatValue: aFloat];
	} else {
		[displayedValues setObject: [NSNumber numberWithFloat: aFloat] forKey: ZLight];
	}

	[displayedValues setObject: ([showBSPOutputButton state] ? yes : no) forKey: ShowBSPOutput];
	[displayedValues setObject: ([offsetBrushCopyButton state] ? yes : no) forKey: OffsetBrushCopy];

	[self commitDisplayedValues];
}

- (void) commitDisplayedValues
{
	if (currentValues != displayedValues) {
		[currentValues release];
		currentValues = [displayedValues copyWithZone: [self zone]];
	}
}

- (void) discardDisplayedValues
{
	if (currentValues != displayedValues) {
		[displayedValues release];
		displayedValues = [currentValues mutableCopyWithZone: [self zone]];
		[self updateUI];
	}
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

	currentValues = [defaultValues () copyWithZone: [self zone]];
	[currentValues retain];

	[self discardDisplayedValues];
}

/***
	Code to deal with defaults
***/

#define getBoolDefault(name) \
{ \
	NSString *str = [defaults stringForKey: name]; \
	[dict setObject: (str ? [NSNumber numberWithBool: [str hasPrefix: @"Y"]] : [defaultValues() objectForKey: name]) forKey: name]; \
}

#define getFloatDefault(name) \
{ \
	NSString *str = [defaults stringForKey: name]; \
	[dict setObject: (str ? [NSNumber numberWithFloat: [str floatValue]] : [defaultValues() objectForKey: name]) forKey: name]; \
}

#define getIntDefault(name) \
{ \
	NSString *str = [defaults stringForKey: name]; \
	[dict setObject: (str ? [NSNumber numberWithInt: [str intValue]] : [defaultValues() objectForKey: name]) forKey: name]; \
}

#define getStringDefault(name) \
{ \
	NSString *str = [defaults stringForKey: name]; \
	[dict setObject: (str ? str : [defaultValues() objectForKey: name]) forKey: name]; \
}

+ (NSDictionary *) preferencesFromDefaults
{
	NSUserDefaults		*defaults = [NSUserDefaults standardUserDefaults];
	NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity: 10];

	getStringDefault(ProjectPath);
	getStringDefault(BspSoundPath);
	getIntDefault(StartWad);
	getFloatDefault(XLight);
	getFloatDefault(YLight);
	getFloatDefault(ZLight);
	getBoolDefault(ShowBSPOutput);
	getBoolDefault(OffsetBrushCopy);

	return dict;
}

#define setBoolDefault(name) \
{ \
	if ([[defaultValues() objectForKey: name] isEqual: [dict objectForKey: name]]) \
		[defaults removeObjectForKey: name]; \
	else \
		[defaults setBool:[[dict objectForKey:name] boolValue] forKey: name]; \
}

#define setFloatDefault(name) \
{ \
	if ([[defaultValues() objectForKey: name] isEqual: [dict objectForKey: name]]) \
		[defaults removeObjectForKey: name]; \
	else \
		[defaults setFloat:[[dict objectForKey:name] floatValue] forKey: name]; \
}

#define setIntDefault(name) \
{ \
	if ([[defaultValues() objectForKey:name] isEqual:[dict objectForKey:name]]) \
		[defaults removeObjectForKey:name]; \
	else \
		[defaults setInteger:[[dict objectForKey:name] intValue] forKey:name]; \
}

#define setStringDefault(name) \
{ \
	if ([[defaultValues() objectForKey:name] isEqual: [dict objectForKey: name]]) \
		[defaults removeObjectForKey: name]; \
	else \
		[defaults setObject: [[dict objectForKey: name] stringValue] forKey: name]; \
}

+ (void) savePreferencesToDefaults: (NSDictionary *) dict
{
	NSUserDefaults	*defaults = [NSUserDefaults standardUserDefaults];

	setStringDefault(ProjectPath);
	setStringDefault(BspSoundPath);
	setIntDefault(StartWad);
	setFloatDefault(XLight);
	setFloatDefault(YLight);
	setFloatDefault(ZLight);
	setBoolDefault(ShowBSPOutput);
	setBoolDefault(OffsetBrushCopy);
}

@end
