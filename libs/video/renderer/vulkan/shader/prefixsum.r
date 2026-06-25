#define uint32_t uint
#include "../../../../../include/QF/math/bitop.h"

void printf (string fmt, ...)
	= @intrinsic(OpExtInst, "NonSemantic.DebugPrintf", DebugPrintf);

#define workgroup_size 512
#define block_size (workgroup_size * 2)

[shared] uint local_data[block_size];

[push_constant] @block Params {
	uint *in_data;
	uint *out_data;
	uint *sum_data;
	uint count;
};

[in("GlobalInvocationId")] uvec3 gl_GlobalInvocationID;
[in("LocalInvocationId")] uvec3 gl_LocalInvocationID;

void barrier () = @intrinsic(OpControlBarrier)
	[Scope.Workgroup, Scope.Workgroup,
	 (MemorySemantics.AcquireRelease | MemorySemantics.WorkgroupMemory)];

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
main ()
{
	const uint num_steps = BITOP_LOG2(workgroup_size) + 1;
	uint ext_ind0 = gl_GlobalInvocationID.x * 2 + 0;
	uint ext_ind1 = gl_GlobalInvocationID.x * 2 + 1;
	uint id = gl_LocalInvocationID.x;
	uint loc_ind0 = gl_LocalInvocationID.x * 2 + 0;
	uint loc_ind1 = gl_LocalInvocationID.x * 2 + 1;

	local_data[loc_ind0] = (ext_ind0 < count) ? in_data[ext_ind0] : 0;
	local_data[loc_ind1] = (ext_ind1 < count) ? in_data[ext_ind1] : 0;
	barrier ();

	if (id < 16 && ext_ind0 < 1024) {
		printf ("%u %u %u\n", id, local_data[loc_ind0], local_data[loc_ind1]);
	}

	// inclusive prefix sum using Blelloch
	for (uint step = 0; step < num_steps; step++) {
		uint mask = (1 << step) - 1;
		uint rd_ind = ((id >> step) << (step + 1)) + mask;
		uint wr_ind = rd_ind + 1 + (id & mask);

		local_data[wr_ind] += local_data[wr_ind];

		barrier ();
	}

	if (ext_ind0 < count) {
		// output data converting to exclusive prefix sum
		if (id < workgroup_size - 1) {
			out_data[ext_ind0 + 1] = local_data[loc_ind0];
			out_data[ext_ind1 + 1] = local_data[loc_ind1];
		} else {
			out_data[ext_ind0 & ~(block_size - 1)] = 0;
			sum_data[ext_ind0 / block_size] = local_data[loc_ind1];
		}
	}
}

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
offset ()
{
	uint ext_ind0 = gl_GlobalInvocationID.x * 2 + 0;
	uint ext_ind1 = gl_GlobalInvocationID.x * 2 + 1;
	uint sum_ind = ext_ind0 / block_size;
	if (ext_ind0 < count) {
		out_data[ext_ind0] = in_data[ext_ind0] + sum_data[sum_ind];
	}
	if (ext_ind1 < count) {
		out_data[ext_ind1] = in_data[ext_ind1] + sum_data[sum_ind];
	}
}
