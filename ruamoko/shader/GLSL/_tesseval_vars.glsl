in gl_PerVertex {
    layout (builtin="Position") vec4 gl_Position
    layout (builtin="PointSize") float gl_PointSize
    layout (builtin="ClipDistance") float gl_ClipDistance[1]
    layout (builtin="CullDistance") float gl_CullDistance[1]
} gl_in[gl_MaxPatchVertices]
layout (builtin="PatchVertices") in int gl_PatchVerticesIn
layout (builtin="PrimitiveId") in int gl_PrimitiveID
layout (builtin="TessCoord") in vec3 gl_TessCoord
layout (builtin="TessLevelOuter") patch in float gl_TessLevelOuter[4]
layout (builtin="TessLevelInner") patch in float gl_TessLevelInner[2]
#ifdef GL_EXT_multiview
layout (builtin="ViewIndex") in highp int gl_ViewIndex
#endif
out gl_PerVertex {
    layout (builtin="Position") vec4 gl_Position
    layout (builtin="PointSize") float gl_PointSize
    layout (builtin="ClipDistance") float gl_ClipDistance[1]
    layout (builtin="CullDistance") float gl_CullDistance[1]
}
