#include "general.h"

[uniform, set(0), binding(0)] @block
#include "matrices.h"
;

[push_constant] @block PushConstants {
	vec2 frag_size;
};

[in(0)] vec3 vert_pos;
[in(1)] vec3 vert_norm;
[in(2)] uint vert_id;

[out(0)] uint obj_id;

[out("Position")] vec4 gl_Position;
[in("ViewIndex")] int gl_ViewIndex;

[shader("Vertex")]
[capability("MultiView")]
void
main ()
{
	vec4 pos = Projection3d * (View[gl_ViewIndex] * vec4 (vert_pos, 1));
	vec2 dir = (View[gl_ViewIndex] * vec4 (vert_norm, 0)).xy;
	vec2 ofs = mix (mix(vec2(0,0), vec2(1,1), dir > vec2(0,0)), vec2(-1,-1), dir < vec2(0,0));
	pos = vec4 (pos.xy + ofs * frag_size, pos.zw);
	gl_Position = pos;
	obj_id = vert_id;
}
