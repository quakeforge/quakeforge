#include "atomic.h"
#include "image.h"

[in("ViewIndex"), flat] int gl_ViewIndex;
[in("FragCoord")] vec4 gl_FragCoord;
[in(0), flat] uint obj_id;

#include "queueobj.h"

#define SPV(op) @intrinsic(op)
void begin_interlock() = SPV(OpBeginInvocationInterlockEXT);
void end_interlock() = SPV(OpBeginInvocationInterlockEXT);
#undef SPV

[shader("Fragment", "PixelInterlockUnorderedEXT")]
[extension("SPV_EXT_fragment_shader_interlock")]
[capability("MultiView")]
[capability("FragmentShaderPixelInterlockEXT")]
void
main (void)
{
	uint index = atomicAdd (numObjects, 1);
	ivec3       coord = ivec3(gl_FragCoord.xy, gl_ViewIndex);
	if (index < maxObjects) {
		begin_interlock ();
		uint prevIndex = imageAtomicExchange (queue_heads, coord, index);
		end_interlock ();
		queue[index].obj_id = obj_id;
		queue[index].next = prevIndex;
	}
}
