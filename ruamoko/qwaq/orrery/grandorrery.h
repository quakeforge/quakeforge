#ifndef __qwaq_orrery_grandorrery_h
#define __qwaq_orrery_grandorrery_h

#include <Object.h>

const double AU=149'597'870'700.0;
const double G=6.6743e-11;

@class Array;
@class Body;
@class Orbiter;

@interface GrandOrrery : Object
{
	Array *objects;
	Orbiter *central_object;
}
+(GrandOrrery *) orrery:(PLItem *) plitem;
-(Body *)findBody:(string)name;
-update:(double) time;
@end

#endif//__qwaq_orrery_grandorrery_h
