#ifndef __rua_entity_h
#define __rua_entity_h

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

#endif//__rua_entity_h
