#ifndef __qfcc_shader_glsl_control_h
#define __qfcc_shader_glsl_control_h

#ifndef __GLSL__
#include "_defines.h"
#endif

#if 0
//shader invocation control functions
void barrier()

//shader memory control functions
void memoryBarrier()
void memoryBarrierAtomicCounter()
void memoryBarrierBuffer()
void memoryBarrierImage()
#if defined(__GLSL_COMPUTE__)
void memoryBarrierShared()
void groupMemoryBarrier()
#endif

//shader invocation group functions
bool anyInvocation(bool value)
bool allInvocations(bool value)
bool allInvocationsEqual(bool value)
#endif

#endif//__qfcc_shader_glsl_control_h
