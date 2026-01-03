#include "general.h"

#define INPUT_ATTACH_SET 0
#include "input_attach.h"

INPUT_ATTACH(0) subpassInput color;
INPUT_ATTACH(1) subpassInput emission;
INPUT_ATTACH(2) subpassInput depth;
INPUT_ATTACH(3) subpassInput light;

[in("ViewIndex"), flat] int gl_ViewIndex;
[in("FragCoord")] vec4 gl_FragCoord;

#define OIT_SET 1
#include "oit_blend.finc"

#include "fog.finc"

[push_constant] @block PushConstants {
	vec4        fog;
	vec4        camera;
	float       near_plane;
};

[out(0)] vec4 frag_color;

[shader("Fragment")]
[capability("MultiView")]
void
main (void)
{
	vec3        c = subpassLoad (color).rgb;
	vec3        l = subpassLoad (light).rgb;
	vec3        e = subpassLoad (emission).rgb;
	float       d = subpassLoad (depth).x;
	l = l / (l + 1);
	vec3        o = max(BlendFrags (vec4 (c * l + e, 1)).xyz, vec3(0));

	d = d > 0 ? near_plane / d : 1e36;
	o = FogBlend (vec4(o,1), fog, d).xyz;
	o = pow (o, vec3(0.83));//FIXME make gamma correction configurable
	frag_color = vec4 (o, 1);
}
