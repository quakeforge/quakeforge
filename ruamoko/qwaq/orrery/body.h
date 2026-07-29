#ifndef __qwaq_orrery_body_h
#define __qwaq_orrery_body_h

#include "orbiter.h"

@class PLItem;

@interface Body : Orbiter
{
	double mu;
	double radius;
	double rot_speed;
	double rot_epoch;
	double rot_at_epoch;
	dmat3 frame;
}
+(Body *)body:(PLItem *)plitem orrery:(GrandOrrery *)orrery;
-(double)mu;
@end

#endif//__qwaq_orrery_body_h
