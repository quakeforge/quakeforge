#ifndef __axe_h
#define __axe_h

#include "entity.h"
#include "weapon.h"

@interface Axe : Object <Weapon>
{
	Entity owner;
	float damage;
}
-init;
@end

#endif//__axe_h
