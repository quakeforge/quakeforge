#ifndef __ruamoko_entity_h
#define __ruamoko_entity_h

#include "object.h"

@interface Entity : Object
{
	entity		ent;
}

-init;
-initWithEntity:(entity)e;
-free;

-(entity)new;
-(entity)ent;
@end

@extern void (entity e, vector o) setorigin;
@extern void (entity e, string m) setmodel;
@extern void (entity e, vector min, vector max) setsize;
@extern entity () spawn;
@extern void (entity e) remove;
@extern entity (entity start, .string fld, string match) find;
@extern entity (vector org, float rad) findradius;
@extern entity (entity e) nextent;
@extern void (entity e) makestatic;
@extern void (entity e) setspawnparms;

#endif//__ruamoko_entity_h
