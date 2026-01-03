#include "general.h"

[in("ViewIndex"), flat] int gl_ViewIndex;
[in("FragCoord")] vec4 gl_FragCoord;

#define OIT_SET 1
#include "oit_blend.finc"

#define COLOR_ONLY
#include "input_attach.h"

INPUT_ATTACH(0) subpassInput color;

[out(0)] vec4 frag_color;

[shader("Fragment")]
[capability("MultiView")]
void
main (void)
{
	vec3        c;
	vec3        l;
	vec3        e;
	vec3        o;

	c = subpassLoad (color).rgb;
	o = max(BlendFrags (vec4 (c, 1)).xyz, vec3(0));
	o = pow (o, vec3(0.83));//FIXME make gamma correction configurable
	frag_color = vec4 (o, 1);
}
