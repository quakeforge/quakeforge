#include <PropertyList.h>
#include <math.h>
#include <string.h>

#include "grandorrery.h"
#include "orbiter.h"
#include "physorbit.h"
#include "body.h"

void printf(string fmt, ...);

@implementation Orbiter

-initWithPList:(PLItem *)plitem orrery:(GrandOrrery *)orrery
{
	if (!(self = [super init])) {
		return nil;
	}
	name = str_hold ([[plitem getObjectForKey:"name"] string]);
	parent = [[orrery findBody: [[plitem getObjectForKey:"parent"] string]]
			  retain];
	color = 0xfffffff;	//white
	if ([plitem getObjectForKey:"color"]) {
		color = (uint) [[plitem getObjectForKey:"color"] number];
	}
	if (parent) {
		[parent addChild: self];
	}
	auto o = [plitem getObjectForKey:"orbit"];
	if (o) {
		orbit = [[PhysicalOrbit orbit:o] retain];
	}

	return self;
}

+(Orbiter *)orbiter:(PLItem *)plitem orrery:(GrandOrrery *)orrery
{
	if ([plitem getObjectForKey:"radius"]) {
		return [Body body:plitem orrery:orrery];
	} else {
		return [[[Orbiter alloc] initWithPList:plitem orrery:orrery]
				autorelease];
	}
}

-addChild:(Orbiter *)child
{
	if (!children) {
		children = [[Array array] retain];
	}
	[children addObject:child];
	return self;
}

-(PhysicalOrbit *)orbit
{
	return orbit;
}

-(conic_t)conicData
{
	@algebra (PGA) {
		conic_t data = {
			.s = e032,
			.t = e013,
		};
		if (orbit) {
			auto pos = dvec3(0);
			pos = [parent position];
			data = [orbit conicData];
			data.point = point_t (pos, 1);
			data.width = 2;
			data.color = color;
		}
		return data;
	}
}

-updatePosition:(double)time ref:(Orbiter *)ref
{
	if (ref == self) {
		// this is the origin (root) body
		position = dvec3(0);
		if (parent) {
			[parent updatePosition:time ref:self];
		}
		for (int i = [children count]; i-- > 0; ) {
			[[children objectAtIndex:i] updatePosition:time ref:self];
		}
	} else if (ref == parent) {
		// traveling down the tree
		// position is relative to the parent
		position = ref.position + [orbit position:time];
		for (int i = [children count]; i-- > 0; ) {
			[[children objectAtIndex:i] updatePosition:time ref:self];
		}
	} else {
		// traveling up the tree
		// position is relative to the child
		position = ref.position - [[ref orbit] position:time];
		if (parent) {
			[parent updatePosition:time ref:self];
		}
		for (int i = [children count]; i-- > 0; ) {
			Orbiter *child = [children objectAtIndex:i];
			if (child != ref) {
				[child updatePosition:time ref:self];
			}
		}
	}
	return self;
}

-(string)name
{
	return name;
}

-(dvec3)position
{
	return position;
}

-(string)describe
{
	return sprintf("%s:%s", [super describe], name);
}

-(vec4)color
{
	return vec4((color>>0)&0xff, (color>>8)&0xff, (color>>16)&0xff, (color>>24)&0xff)/255;
}
@end
