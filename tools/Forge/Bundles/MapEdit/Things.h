#ifndef Things_h
#define Things_h

#include <AppKit/AppKit.h>

#include "Entity.h"

extern id  things_i;

#define ENTITYNAMEKEY "spawn"

@interface Things: NSObject
{
	id  entity_browser_i;           // browser
	id  entity_comment_i;           // scrolling text window

	id  prog_path_i;

	int  lastSelected;              // last row selected in browser

	id  keyInput_i;
	id  valueInput_i;
	id  flags_i;
}

- (id) initEntities;

- (id) newCurrentEntity;
- (id) setSelectedKey: (epair_t *)ep;

- (id) clearInputs;
- (const char *) spawnName;

// UI targets
- (id) reloadEntityClasses: sender;
- (id) selectEntity: sender;
- (id) doubleClickEntity: sender;

// Action methods
- (id) addPair: sender;
- (id) delPair: sender;
- (id) setAngle: sender;
- (id) setFlags: sender;

@end
#endif // Things_h
