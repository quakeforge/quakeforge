// workgroup dimensions
layout (builtin="NumWorkgroups") in uvec3 gl_NumWorkGroups;
layout (builtin="WorkgroupSize") in uvec3 gl_WorkGroupSize;
// workgroup and invocation IDs
layout (builtin="WorkgroupId") in uvec3 gl_WorkGroupID;
layout (builtin="LocalInvocationId") in uvec3 gl_LocalInvocationID;
// derived variables
layout (builtin="GlobalInvocationId") in uvec3 gl_GlobalInvocationID;
layout (builtin="LocalInvocationIndex") in uint gl_LocalInvocationIndex;
