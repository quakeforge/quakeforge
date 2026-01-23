#include "GLSL/atomic.h"
#include "GLSL/image.h"

[in("ViewIndex"), flat] int gl_ViewIndex;
[in("FrontFacing")] bool gl_FrontFacing;
[in("FragCoord")] vec4 gl_FragCoord;

#include "oit_store.finc"

[in(0), flat] uint light_index;

[early_fragment_tests, in];

[shader("Fragment")]
[capability("MultiView")]
void
main (void)
{
	vec4 c = !gl_FrontFacing ? vec4 (1, 0, 1, 0.05) : vec4 (0, 1, 1, 0.05);
	StoreFrag (c, gl_FragCoord.z);
}
