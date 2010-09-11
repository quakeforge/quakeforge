#ifndef Entity_h
#define Entity_h

#include <AppKit/AppKit.h>

#include "QF/mathlib.h"

typedef struct epair_s {
	struct epair_s *next;
	char       *key;
	char       *value;
} epair_t;

// an Entity is a list of brush objects, with additional key / value info

@interface Entity:NSMutableArray {
	epair_t    *epairs;
	BOOL        modifiable;
}

-initClass:(char *) classname;
-initFromScript:(struct script_s *) script;

-(void) dealloc;

-(BOOL) modifiable;
-setModifiable:(BOOL) m;

-(char *) targetname;

-writeToFILE:(FILE *)
f           region:(BOOL) reg;

-(char *) valueForQKey:(char *) k;
-getVector:(vec3_t)
v           forKey:(char *) k;

-setKey:(const char *)
k           toValue:(const char *) v;

-(int) numPairs;
-(epair_t *) epairs;
-removeKeyPair:(char *) key;

@end
#endif // Entity_h
