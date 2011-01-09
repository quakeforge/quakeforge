#ifndef __Axe_h
#define __Axe_h

#include "Object.h"
#include "Entity.h"
#include "Weapon.h"

@interface Axe: Object <Weapon>
{
	Entity[]	owner;
	float	damage;
}

- (id) init;

@end

#endif	//__Axe_h
