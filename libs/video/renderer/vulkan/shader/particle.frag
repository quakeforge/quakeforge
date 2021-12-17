#version 450

layout (location = 0) in vec4 uv_tr;
layout (location = 1) in vec4 color;

layout (location = 0) out vec4 frag_color;

void
main (void)
{
	vec4        c = color;
	vec2        x = uv_tr.xy;

	float       a = 1 - dot (x, x);
	if (a <= 0) {
		discard;
	}
	c.a *= sqrt (a);
	frag_color = c;
}
