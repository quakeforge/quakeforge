/*
	Things.h

	(description)

	Copyright (C) 2001 Jeff Teunissen <deek@d2dc.net>

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
#ifdef HAVE_CONFIG_H
# include "Config.h"
#endif

#import <AppKit/AppKit.h>

extern	id	things_i;

#define	ENTITYNAMEKEY	"spawn"

@interface Things: Object
{
	id	entity_browser_i;	// browser
	id	entity_comment_i;	// scrolling text window
	
	id	prog_path_i;
	
	int	lastSelected;	// last row selected in browser

	id	keyInput_i;
	id	valueInput_i;
	id	flags_i;
}

- initEntities;

- newCurrentEntity;
- setSelectedKey:(epair_t *)ep;

- clearInputs;
- (char *)spawnName;

// UI targets
- reloadEntityClasses: sender;
- selectEntity: sender;
- doubleClickEntity: sender;

// Action methods
- addPair:sender;
- delPair:sender;
- setAngle:sender;
- setFlags:sender;


@end
