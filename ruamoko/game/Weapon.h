#ifndef __Weapon_h
#define __Weapon_h

#include "Entity.h"

@protocol Weapon

- (void) setOwner: (Entity *)o;
- (void) fire;

@end

#endif //__Weapon_h
