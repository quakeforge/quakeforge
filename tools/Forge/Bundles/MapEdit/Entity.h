#ifndef Entity_h
#define Entity_h

#include <AppKit/AppKit.h>

#include "QF/mathlib.h"
#include "QF/quakeio.h"

typedef struct  epair_s {
	struct epair_s  *next;
	struct epair_s **prev;
	char            *key;
	char            *value;
} epair_t;

// an Entity is a list of brush objects, with additional key / value info

@interface Entity: NSMutableArray
{
	NSMutableArray  *array;
	epair_t         *epairs;
	struct hashtab_s *epair_tab;
	BOOL            modifiable;
}

- (Entity *) initClass: (const char *)classname;
- (Entity *) initFromScript: (struct script_s *)script;

- (oneway void) dealloc;

- (BOOL) modifiable;
- (void) setModifiable: (BOOL)m;

- (const char *) targetname;

- (void) writeToFile: (QFile *)file region: (BOOL)reg;

- (const char *) valueForQKey: (const char *)k;
- (void) getVector: (vec3_t)v forKey: (const char *)k;

- (void) setKey: (const char *)k
   toValue: (const char *)v;

- (int) numPairs;
- (epair_t *) epairs;
- (void) removeKeyPair: (const char *)key;

@end
#endif // Entity_h
