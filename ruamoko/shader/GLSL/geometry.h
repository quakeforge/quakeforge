#ifndef __qfcc_shader_glsl_geometry_h
#define __qfcc_shader_glsl_geometry_h

//geometry shader functions
void EmitStreamVertex(int stream) = SPV(OpEmitStreamVertex);
void EndStreamPrimitive(int stream) = SPV(OpEndStreamPrimitive);
void EmitVertex() = SPV(OpEmitVertex);
void EndPrimitive() = SPV(OpEndPrimitive);

#endif//__qfcc_shader_glsl_geometry_h
