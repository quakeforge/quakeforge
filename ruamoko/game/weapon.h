#ifndef __weapon_h
#define __weapon_h

#include "entity.h"

@protocol Weapon
-setOwner:(Entity)o;
-fire;
@end

#endif//__weapon_h
