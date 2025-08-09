#include "atomic.h"
#include "image.h"

[in("ViewIndex"), flat] int gl_ViewIndex;
[in("FragCoord")] vec4 gl_FragCoord;
[in(0), flat] uint obj_id;

#include "queueobj.h"

[shader("Fragment")]
[capability("MultiView")]
void
main (void)
{
	uint index = atomicAdd (numObjects, 1);
	ivec3       coord = ivec3(gl_FragCoord.xy, gl_ViewIndex);
	if (index < maxObjects) {
		uint prevIndex = imageAtomicExchange (queue_heads, coord, index);
		queue[index].obj_id = obj_id;
		queue[index].next = prevIndex;
	}
}
