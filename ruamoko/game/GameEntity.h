#ifndef __GameEntity_h
#define __GameEntity_h

#include "Entity.h"
#include "entities.h"

@extern .vector angles;
@extern .float modelindex;
@extern .float movetype;
@extern .string model;
@extern .float frame;
@extern .float colormap;
@extern .vector mins;
@extern .vector maxs;
@extern .vector velocity;
@extern .vector origin;
@extern .float flags;
@extern .vector v_angle;

@interface GameEntity: Entity

- (BOOL) takeDamage: weapon :inflictor :attacker : (float)damage;

@end

#endif	//__GameEntity_h
