layout (builtin="VertexIndex") in int gl_VertexIndex;
layout (builtin="InstanceIndex") in int gl_InstanceIndex;
#ifdef GL_EXT_multiview
layout (builtin="ViewIndex") in highp int gl_ViewIndex;
#endif
out gl_PerVertex {
    layout (builtin="Position") vec4 gl_Position;
    layout (builtin="PointSize") float gl_PointSize;
    layout (builtin="ClipDistance") float gl_ClipDistance[1];
    layout (builtin="CullDistance") float gl_CullDistance[1];
};
