#include <GLSL/atomic.h>

#include "bsp.h"

@namespace cluster {

[in("GlobalInvocationId")] uvec3 gl_GlobalInvocationID;

[push_constant] @block Params {
	uint *command_counts;
	uint *command_offsets;
	command_t *commands;
	bsp_cluster_t *subclusters;
	cluster_t *clusters;
	uint *cluster_map;
	bsp_queue_t *cluster_queue;
	uint cluster_count;
	uint texture_count;
};

uint
alloc_command (const uint tex_id)
{
	return atomicAdd (command_counts[tex_id], 1);
}

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
main ()
{
	uint queue_index = gl_GlobalInvocationID.x;
	if (queue_index >= cluster_count) {
		return;
	}
	auto queue = cluster_queue[queue_index];
	auto cluster = clusters[queue.cluster];
	for (uint i = 0; i < cluster.count; i++) {
		uint subcluster_ind = cluster_map[cluster.first + i];
		auto subcluster = subclusters[subcluster_ind];
		uint command_ind = alloc_command (subcluster.tex_id);
		command_ind += command_offsets[subcluster.tex_id];
		commands[command_ind] = (command_t) {
			.indexCount = subcluster.index_count,
			.instanceCount = queue.instance_count,
			.firstIndex = subcluster.first_index,
			.vertexOffset = 0,
			.firstInstace = queue.first_instance,
		};
	}
}


[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
clear ()
{
	uint tex_id = gl_GlobalInvocationID.x;
	if (tex_id < texture_count) {
		command_counts[tex_id] = 0;
	}
}

}
