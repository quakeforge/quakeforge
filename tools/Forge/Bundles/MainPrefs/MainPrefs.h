/*
	MainPrefs.m

	Controller class for this bundle

	Copyright (C) 2001 Dusk to Dawn Computing, Inc.
	Additional copyrights here

	Author: Jeff Teunissen <deek@d2dc.net>
	Date:	24 Nov 2001

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

#import <AppKit/NSNibDeclarations.h>

#import "BundleController.h"
#import "PrefsView.h"

#define ProjectPath	@"projectPath"

@interface MainPrefs: NSObject <PrefsViewController, ForgeBundle>
{
	IBOutlet id		projectPathField;
	IBOutlet id		projectPathFindButton;

	IBOutlet id		window;
	IBOutlet id		view;
}

@end
