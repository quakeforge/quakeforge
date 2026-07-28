#ifndef __qwaq_shader_common_h
#define __qwaq_shader_common_h

void printf (string fmt, ...)
	= @intrinsic(OpExtInst, "NonSemantic.DebugPrintf", DebugPrintf);

#define INPUT_ATTACH(ind) \
	[uniform, input_attachment_index(ind), set(0), binding(ind)] \
	@image(float, SubpassData)

#endif//__qwaq_shader_common_h
