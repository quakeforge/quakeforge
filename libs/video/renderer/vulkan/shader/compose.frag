#version 450

layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput opaque;
layout (input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput translucent;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec3        o;
	vec4        t;
	vec3        c;

	o = subpassLoad (opaque).rgb;
	t = subpassLoad (translucent);
	c = mix (o, t.rgb, t.a);
	frag_color = vec4 (c, 1);
}
