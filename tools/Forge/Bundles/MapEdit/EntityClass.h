#ifndef EntityClass_h
#define EntityClass_h

#include <AppKit/AppKit.h>

#include "QF/mathlib.h"

typedef enum { esize_model, esize_fixed } esize_t;

#define	MAX_FLAGS	8

@interface EntityClass:NSObject
{
	char       *name;
	esize_t     esize;
	vec3_t      mins, maxs;
	vec3_t      color;
	char       *comments;
	char       *flagnames[MAX_FLAGS];
}

-initFromText:(const char *)text source:(const char *) filename;

-(char *) classname;
-(esize_t) esize;
-(float *) mins;						// only for esize_fixed
-(float *) maxs;						// only for esize_fixed
-(float *) drawColor;
-(char *) comments;
-(char *) flagName:(unsigned) flagnum;

@end extern id entity_classes_i;

@interface EntityClassList:NSMutableArray
{
	NSMutableArray *array;
	id          nullclass;
	char       *source_path;
}

-initForSourceDirectory:(char *) path;
-(id) classForName:(const char *) name;
-(void) scanDirectory;

@end
#endif // EntityClass_h
