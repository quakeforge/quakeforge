#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput depth;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput color;
layout (input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput normal;

struct LightData {
	vec3        color;
	float       dist;
	vec3        position;
	int         type;
	vec3        direction;
	float       cone;
};

layout (constant_id = 0) const int MaxLights = 8;
/*layout (set = 1, binding = 0) uniform Lights {
	int         lightCount;
	LightData   lights[MaxLights];
};*/

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	float       d = subpassLoad (depth).r;
	vec4        c;

	//c = vec4 (d, d, d, 1);
	c = vec4 (subpassLoad (color).rgb, 1);
	frag_color = c;
}
