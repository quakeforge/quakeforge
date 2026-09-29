#include <GLSL/atomic.h>

#include "bsp.h"

#include "entity.h"
typedef struct Entity Entity;//FIXME eliminate glsl uses

void printf (string fmt, ...)
	= @intrinsic(OpExtInst, "NonSemantic.DebugPrintf", DebugPrintf);

uint subgroup_max (uint x) = @intrinsic(OpGroupNonUniformUMax)
	[Scope.Subgroup, =GroupOperation.Reduce, x];

[in("GlobalInvocationId")] uvec3 gl_GlobalInvocationID;

typedef struct cluster_queue_s {
	uint        count;
	uint        x, y, z;
	bsp_queue_t queue[];//not really FIXME need to decorate with run time array
} cluster_queue_t;

@namespace cluster {

[push_constant] @block Params {
	uint       *command_counts;
	uint       *command_offsets;
	bsp_command_t *commands;
	bsp_cluster_t *subclusters;
	cluster_t  *clusters;
	uint       *cluster_map;
	cluster_queue_t *cluster_queue;
	uint        texture_count;
	uint        anim_index;

	bsp_model_t *models;
	uint       *tex_ids;

	bsp_texanim_t *anim_main;	///< group 0 animations
	bsp_texanim_t *anim_alt;	///< group 1 animations
	ushort     *frame_map;		///< map from texture frame to texture id
};

[capability(GroupNonUniformArithmetic)]
[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
main ()
{
	uint queue_index = gl_GlobalInvocationID.x;
	bsp_queue_t *queue = nil;
	cluster_t *cluster = nil;
	if (queue_index < cluster_queue.count) {
		queue = &cluster_queue.queue[queue_index];
		cluster = &clusters[queue.cluster];
	}
	uint count = 0;
	uint trans = 0;
	bsp_texanim_t *frame_anim = anim_main;
	if (cluster) {
		count = cluster.count;
		frame_anim = queue.frame ? anim_alt : anim_main;
		trans = queue.trans * texture_count;
	}
	uint maxCount = subgroup_max (count);
	for (uint i = 0; i < maxCount; i++) {
		if (i < count) {
			uint subcluster_ind = cluster_map[cluster.first + i];
			auto subcluster = subclusters[subcluster_ind];
			uint tex_id = subcluster.tex_id;
			auto anim = frame_anim[tex_id];
			uint anim_ind = (anim_index + anim.offset) % anim.count;
			tex_id = frame_map[anim.base + anim_ind] + trans;
			uint command_ind = atomicAdd (command_counts[tex_id], 1);
			command_ind += command_offsets[tex_id];
			commands[command_ind] = (bsp_command_t) {
				.indexCount = subcluster.index_count,
				.instanceCount = queue.instance_count,
				.firstIndex = subcluster.first_index,
				.vertexOffset = 0,
				.firstInstace = queue.first_instance,
			};
		}
	}
}

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
clear ()
{
	uint tex_id = gl_GlobalInvocationID.x;
	if (tex_id < 2 * texture_count) {
		command_counts[tex_id] = 0;
	}
}

}

@namespace ent {

[push_constant] @block Params {
	uint       *ent_ids;
	uint       *inst_ids;
	Entity     *entities;
	cluster_queue_t *cluster_queue;
	bsp_model_t *models;

	uint       *mod_counts;
	uint       *mod_offsets;
	uint        ent_count;
	uint        num_models;
};

[shader(GLCompute, LocalSize=[1,1,1])]
void
set_dispatch ()
{
	if (cluster_queue) {
		cluster_queue.x = (cluster_queue.count + workgroup_size - 1)
						/ workgroup_size;
	}
}

[shader(GLCompute, LocalSize=[1,1,1])]
void
copy_offset ()
{
	if (num_models) {
		uint end = num_models - 1;
		uint total = mod_offsets[end] + mod_counts[end];
		mod_offsets[num_models] = total;
		mod_counts[num_models] -= total;
	}
}

[shader(GLCompute, LocalSize=[1,1,1])]
void
copy_total ()
{
	if (num_models) {
		uint end = num_models - 1;
		uint total = mod_offsets[end] + mod_counts[end];
		mod_counts[num_models] += total;
	}
}

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
distribute ()
{
	uint ent_ind = gl_GlobalInvocationID.x;
	if (ent_ind < 1 || ent_ind >= ent_count) {
		return;
	}
	uint ent_id = ent_ids[ent_ind];

	uint mod_id = entities[ent_id].model;
	uint frame = entities[ent_id].frame & 1;
	uint trans = entities[ent_id].color[3] < 1 ? 2 : 0;
	uint mod_base = (frame + trans) * num_models;

	uint inst_ind = atomicAdd (mod_offsets[mod_base + mod_id], 1);
	inst_ids[inst_ind] = ent_id;
}

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
enqueue ()
{
	uint mod_id = gl_GlobalInvocationID.x;
	bsp_model_t mod = nil;
	if (mod_id >= 1 && mod_id < num_models) {
		mod = models[mod_id];
	}

	for (uint j = 0; j < mod_queues && mod.cluster_count; j++) {
		uint mod_base = j * num_models;
		uint first_instance = mod_offsets[mod_base + mod_id];
		uint instance_count = mod_counts[mod_base + mod_id];

		if (instance_count) {
			uint count = mod.cluster_count;
			uint maxCount = subgroup_max (count);
			for (uint i = 0; i < maxCount; i++) {
				if (i < count) {
					uint index = atomicAdd (cluster_queue.count, 1);
					bsp_queue_t q = {
						.cluster = mod.first_cluster + i,
						.first_instance = first_instance,
						.instance_count = instance_count,
						//FIXME qfcc doesn't warn (produces bad spir-v
						//without cast)
						.frame = (ushort) (j & 1),
						.trans = (ushort) ((j >> 1) & 1),
					};
					cluster_queue.queue[index] = q;
				}
			}
		}
	}
}

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
count ()
{
	uint ent_ind = gl_GlobalInvocationID.x;
	if (ent_ind < 1 || ent_ind >= ent_count) {
		mod_counts[0] = 1;
		return;
	}
	uint ent_id = ent_ids[ent_ind];

	uint mod_id = entities[ent_id].model;
	uint frame = entities[ent_id].frame & 1;
	uint trans = entities[ent_id].color[3] < 1 ? 2 : 0;
	uint mod_base = (frame + trans) * num_models;

	atomicAdd (mod_counts[mod_base + mod_id], 1);
}

[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
void
clear ()
{
	uint mod_id = gl_GlobalInvocationID.x;
	if (mod_id < num_models * mod_queues) {
		mod_counts[mod_id] = 0;
	}
}

}
