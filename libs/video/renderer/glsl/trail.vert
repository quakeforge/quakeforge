attribute vec4 last, current, next;
attribute vec3 barycentric;
attribute float texoff;

uniform mat4 proj, view;
uniform vec2 viewport;
uniform float width;

varying vec2 texcoord;
varying vec3 vBC;

vec4
transform (vec3 coord)
{
	return proj * view * vec4 (coord, 1.0);
}

vec2
project (vec4 coord)
{
	vec3        device = coord.xyz / coord.w;
	vec2        clip = (device * 0.5 + 0.5).xy;
	return clip * viewport;
}

vec4
unproject (vec2 screen, float z, float w)
{
	vec2        clip = screen / viewport;
	vec2        device = clip * 2.0 - 1.0;
	return vec4 (device * w, z, w);
}

float
estimateScale (vec3 position, vec2 sPosition)
{
	vec4        view_pos = view * vec4 (position, 1.0);
	vec4        scale_pos = view_pos - vec4 (normalize (view_pos.xy) * width,
											 0.0, 0.0);
	vec2        screen_scale_pos = project (proj * scale_pos);
	return distance (sPosition, screen_scale_pos);
}

void
main (void)
{
	vec2        sLast = project (transform (last.xyz));
	vec2        sNext = project (transform (next.xyz));
	vec4        dCurrent = transform (current.xyz);
	vec2        sCurrent = project (dCurrent);
	float       off = current.w;

	texcoord = vec2 (texoff * 0.7, off * 0.5 + 0.5);
	vBC = barycentric;

	// FIXME either n1 or n2 could be zero
	vec2        n1 = normalize (sLast - sCurrent);
	vec2        n2 = normalize (sCurrent - sNext);
	// FIXME if n1 == -n2, the vector will be zero
	vec2        n = normalize (n1 + n2);

	// rotate the normal by 90 degrees and scale by the desired width
	vec2        dir = vec2 (n.y, -n.x) * off;
	float       scale = estimateScale (current.xyz, sCurrent);
	vec2        pos = sCurrent + dir * scale;

	gl_Position = unproject (pos, dCurrent.z, dCurrent.w);
}
