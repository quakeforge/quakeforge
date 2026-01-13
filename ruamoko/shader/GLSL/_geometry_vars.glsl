in gl_PerVertex {
    layout (builtin="Position") vec4 gl_Position;
    layout (builtin="PointSize") float gl_PointSize;
    layout (builtin="ClipDistance") float gl_ClipDistance[1];
    layout (builtin="CullDistance") float gl_CullDistance[1];
} gl_in[];
layout (builtin="PrimitiveId") in int gl_PrimitiveIDIn;
layout (builtin="InvocationId") in int gl_InvocationID;
#ifdef GL_EXT_multiview
layout (builtin="ViewIndex") in highp int gl_ViewIndex;
#endif
out gl_PerVertex {
    layout (builtin="Position") vec4 gl_Position;
    layout (builtin="PointSize") float gl_PointSize;
    layout (builtin="ClipDistance") float gl_ClipDistance[1];
    layout (builtin="CullDistance") float gl_CullDistance[1];
};
layout (builtin="PrimitiveId") out int gl_PrimitiveID;
layout (builtin="Layer") out int gl_Layer;
layout (builtin="ViewportIndex") out int gl_ViewportIndex;
