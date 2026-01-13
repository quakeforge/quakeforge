layout (builtin="FragCoord") in vec4 gl_FragCoord;
layout (builtin="FrontFacing") in bool gl_FrontFacing;
layout (builtin="ClipDistance") in float gl_ClipDistance[1];
layout (builtin="CullDistance") in float gl_CullDistance[1];
layout (builtin="PointCoord") in vec2 gl_PointCoord;
layout (builtin="PrimitiveId") in int gl_PrimitiveID;
layout (builtin="SampleId") in int gl_SampleID;
layout (builtin="SamplePosition") in vec2 gl_SamplePosition;
layout (builtin="SampleMask") in int gl_SampleMaskIn[1];
layout (builtin="Layer") in int gl_Layer;
layout (builtin="ViewportIndex") in int gl_ViewportIndex;
layout (builtin="HelperInvocation") in bool gl_HelperInvocation;
#ifdef GL_EXT_multiview
layout (builtin="ViewIndex") in highp flat int gl_ViewIndex;
#endif
layout (builtin="FragDepth") out float gl_FragDepth;
layout (builtin="SampleMask") out int gl_SampleMask[1];
