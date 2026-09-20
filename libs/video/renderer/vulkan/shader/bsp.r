#include <GLSL/atomic.h>

#include "bsp.h"

#include "entity.h"
typedef struct Entity Entity;//FIXME eliminate glsl uses

[in("GlobalInvocationId")] uvec3 gl_GlobalInvocationID;

@namespace cluster {

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
		uint tex_id = subcluster.tex_id;
		uint command_ind = alloc_command (tex_id);
		command_ind += command_offsets[tex_id];
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

@namespace ent {

[push_constant] @block Params {
	uint        ent_count;
	uint        anim_index;
	uint       *ent_ids;
	Entity     *entities;
	bsp_model_t *models;
	uint       *tex_ids;

	bsp_texanim_t *anim_main;	///< group 0 animations
	bsp_texanim_t *anim_alt;	///< group 1 animations
	ushort     *frame_map;		///< map from texture frame to texture id

	uint       *tex_counts;
	uint        num_tex;
};

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
count ()
{
	uint ent_ind = gl_GlobalInvocationID.x;
	if (ent_ind >= ent_count) {
		return;
	}
	uint ent_id = ent_ids[ent_ind];

	uint mod_id = entities[ent_id].model;
	uint frame = entities[ent_id].frame;
	uint tex_base = entities[ent_id].color[3] < 1 ? num_tex : 0;

	uint first_texture = models[mod_id].first_texture;
	uint texture_count = models[mod_id].texture_count;

	auto texanim = frame ? anim_alt : anim_main;

	for (uint i = 0; i < texture_count; i++) {
		uint tex_id = tex_ids[i];
		auto anim = texanim[tex_id];
		uint frame_ind = anim.base + (anim_index + anim.offset) % anim.count;
		tex_id = frame_map[frame_ind];

		atomicAdd (tex_counts[tex_base + tex_id], 1);
	}
}

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
clear ()
{
	uint tex_id = gl_GlobalInvocationID.x;
	if (tex_id < num_tex * 2) {
		tex_counts[tex_id] = 0;
	}
}

}
