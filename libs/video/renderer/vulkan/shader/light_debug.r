[in(0), flat] uint light_index;
[out(0)] vec4 frag_color;
[in("FrontFacing")] bool gl_FrontFacing;

[shader("Fragment")]
[capability("MultiView")]
void
main (void)
{
	frag_color = gl_FrontFacing ? vec4 (1, 0, 1, 1) : vec4 (0, 1, 1, 1);
}
