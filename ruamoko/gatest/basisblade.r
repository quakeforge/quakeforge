#include <string.h>
#include "basisblade.h"
#include "metric.h"
#include "util.h"

@implementation BasisBlade : Object
-(BasisBlade *) initWithMask:(unsigned) mask scale:(double) scale
{
	if (!(self = [super init])) {
		return self;
	}
	self.mask = mask;
	self.scale = scale;
	return self;
}

+(BasisBlade *) scalar:(double) scale
{
	return [[[BasisBlade alloc] initWithMask:0 scale:scale] autorelease];
}

+(BasisBlade *) zero
{
	return [[[BasisBlade alloc] initWithMask:0 scale:0] autorelease];
}

+(BasisBlade *) basis:(unsigned) mask
{
	return [[[BasisBlade alloc] initWithMask:mask scale:1] autorelease];
}

+(BasisBlade *) basis:(unsigned) mask scale:(double) scale
{
	return [[[BasisBlade alloc] initWithMask:mask scale:scale] autorelease];
}

-(BasisBlade *) product:(BasisBlade *) b isOuter:(int)outer metric:(Metric *)m
{
	if (outer && (mask & b.mask)) {
		// the two blades share at least one basis vector
		return [BasisBlade zero];
	}
	if (!scale || !b.scale) {
		return [BasisBlade zero];
	}
	int         sign = 1 - (-(count_flips (mask, b.mask) & 1) & 2);
	double      s = scale * b.scale * sign;
	if (m) {
		s *= [m apply: mask, b.mask];
		if (!s) {
			return [BasisBlade zero];
		}
	}
	return [BasisBlade basis:(mask ^ b.mask) scale:s];
}

-(BasisBlade *) outerProduct:(BasisBlade *) b
{
	return [self product:b isOuter:1 metric:nil];
}

-(BasisBlade *) geometricProduct:(BasisBlade *) b metric:(Metric *) m
{
	return [self product:b isOuter:0 metric:m];
}

-(BasisBlade *) geometricProduct:(BasisBlade *) b
{
	return [self product:b isOuter:0 metric:nil];
}

-(int) grade
{
	return count_bits (mask);
}

-(unsigned) mask
{
	return mask;
}

-(double) scale
{
	return scale;
}

-(string) name
{
	string      basis = "";

	for (int i = 0; i < 32; i++) {
		if (mask & (1 << i)) {
			basis += sprintf("%x", i + 1);
		}
	}
	if (basis) {
		basis = "e" + basis;
	}
	return basis;
}

-(string) describe
{
	return sprintf ("%g%s", scale, [self name]);
}
@end
