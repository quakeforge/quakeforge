#include <GLSL/atomic.h>
#include <GLSL/image.h>

#include "bsp.h"

#include "entity.h"
typedef struct Entity Entity;//FIXME eliminate glsl uses

#define DISPATCH(x) (((x) + workgroup_size - 1) / workgroup_size)
#define BLOCKDISP(x) (((x) + (2 * workgroup_size) - 1) / (2 * workgroup_size))

void printf (string fmt, ...)
	= @intrinsic(OpExtInst, "NonSemantic.DebugPrintf", DebugPrintf);

uint subgroup_max (uint x) = @intrinsic(OpGroupNonUniformUMax)
	[Scope.Subgroup, =GroupOperation.Reduce, x];

[in("GlobalInvocationId")] uvec3 gl_GlobalInvocationID;

typedef struct instance_queue_s {
	uint        count;
	uint        x, y, z;
	bsp_queue_t queue[];
} instance_queue_t;

typedef struct cluster_queue_s {
	uint        count;
	uint        x, y, z;
	uint        queue[];
} cluster_queue_t;

typedef struct light_queue_s {
	uint        count;
	uint        x, y, z;
	// queue data is separate
} light_queue_t;;

@namespace cluster {
	[push_constant] @block Params {
		uint       *command_counts;
		uint       *command_offsets;
		bsp_command_t *commands;
		bsp_cluster_t *subclusters;
		cluster_t  *clusters;
		uint       *cluster_map;
		instance_queue_t *instance_queue;
		uint        texture_count;
		uint        anim_index;

		bsp_model_t *models;
		uint       *tex_ids;

		bsp_texanim_t *anim_main;	///< group 0 animations
		bsp_texanim_t *anim_alt;	///< group 1 animations
		ushort     *frame_map;		///< map from texture frame to texture id
	};

	// Write VkDrawIndexedIndirectCommand (aliased as bsp_command_t) entries
	// for each subcluster for vkCmdDrawIndexedIndirectCount, incrementing
	// the command count. Each sub-queue is identified by texture id, with
	// two sets: set 0 for entities with alpha=1, set 1 for entities with
	// alpha<1. The two sets are separated by texture_count. Texture
	// animations are controled by the global anim_index and the entity's
	// frame (0 or 1). Each cluster is expanded to its subclusters (which
	// specify vertex index range and texture id), and the cluster specifies
	// the instance id range.
	//
	// per queued cluster, indirect dispatch (cluster_queue/instance_queue)
	// FIXME something is very wrong here: CPU uses cluster_queue to invoke,
	// this refers to instance_queue, but changing the cpu code breaks things.
	// FIXME make properly uniform (see lightmap)
	[capability(GroupNonUniformArithmetic)]
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void main ()
	{
		uint queue_index = gl_GlobalInvocationID.x;
		bsp_queue_t *queue = nil;
		cluster_t *cluster = nil;
		if (queue_index < instance_queue.count) {
			queue = &instance_queue.queue[queue_index];
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

	// Reset the command counts for all per-texture cluster queues
	// per texture (doubled), direct dispatch
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void clear ()
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
		instance_queue_t *instance_queue;
		cluster_queue_t *cluster_queue;
		bsp_model_t *models;

		uint       *mod_counts;
		uint       *mod_offsets;
		uint        ent_count;
		uint        num_models;

		uint       *prefixsum_counts;
	};

	// Assign the entity instances to their model sub-queues
	// per entity, direct dispatch
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void distribute ()
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

	// Set up the instance queue entries for all the models except world.
	// The world model (0) was handled by world_instance() as its clusters
	// are controlled by the PVS for the camera cluster.
	// per model, direct dispatch
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void instances ()
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
				uint first = atomicAdd (cluster_queue.count, count);
				for (uint i = 0; i < maxCount; i++) {
					if (i < count) {
						uint index = atomicAdd (instance_queue.count, 1);
						bsp_queue_t q = {
							.cluster = mod.first_cluster + i,
							.first_instance = first_instance,
							.instance_count = instance_count,
							//FIXME qfcc doesn't warn (produces bad spir-v
							//without cast)
							.frame = (ushort) (j & 1),
							.trans = (ushort) ((j >> 1) & 1),
						};
						instance_queue.queue[index] = q;

						cluster_queue.queue[first + i] = q.cluster;
					}
				}
			}
		}
	}

	// Count the entity instances on each model
	// per entity, direct dispatch
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void count ()
	{
		uint ent_ind = gl_GlobalInvocationID.x;
		if (ent_ind == 0) {
			uint count = num_models * mod_queues;
			uint sum_count = BLOCKDISP (count);
			prefixsum_counts[0] = count;
			prefixsum_counts[1] = sum_count;
		}
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

	// Set up the instance queue entries for the visible world clusters
	// The world model is always instance 0, frame 0, and non-translucent
	// per cluster, indirect dispatch (cluster_queue)
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void world_instance ()
	{
		uint ind = gl_GlobalInvocationID.x;
		if (ind < cluster_queue.count) {
			instance_queue.queue[ind] = {
				.cluster = cluster_queue.queue[ind],
				.instance_count = 1,
			};
		}
		if (ind == 0) {
			instance_queue.count = cluster_queue.count;
		}
	}

	// Reset the model instance counts
	// per model, direct dispatch
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void clear ()
	{
		uint mod_id = gl_GlobalInvocationID.x;
		if (mod_id < num_models * mod_queues) {
			mod_counts[mod_id] = 0;
		}
	}

	@namespace set_dispatch {
		[shader(GLCompute, LocalSize=[1,1,1])]
		void inst ()
		{
			if (instance_queue) {
				instance_queue.x = DISPATCH(instance_queue.count);
			}
		}

		[shader(GLCompute, LocalSize=[1,1,1])]
		void cluster ()
		{
			//if (cluster_queue) {
			//	cluster_queue.x = DISPATCH(cluster_queue.count);
			//}
		}
	}
}

@namespace lightmap {
	// Find the largest index of the array with a value <= key.
	// Entries in the array must be sorted (returns last of duplicates)
	uint fbsearch (const uint key, uint *array, const uint count)
	{
		uint left = 0;
		uint right = count - 1;
		uint mid;

		if (!count) {
			return ~0;
		}
		while (left != right) {
			mid = (left + right + 1) / 2;
			if (key < array[mid]) {
				right = mid - 1;
			} else {
				left = mid;
			}
		}
		return (key >= array[left]) ? left : ~0;
	}

	[push_constant] @block Params {
		bsp_lightinfo_t *lightinfo;
		bsp_surfinfo_t *surfinfo;
		short      *light_style_values;
		byte       *lightmap_data;
		uint       *light_queue_offs;
		uint       *light_queue_inds;
		cluster_t  *light_clusters;
		uint       *light_cluster_surfs;
		cluster_queue_t *cluster_queue;
		light_queue_t *light_queue;

		uint       *prefixsum_counts;
		uint        num_lightmaps;
	};

	[uniform, set(0), binding(0)]
	@image(float, 2D, Storage, Rgba32f) light_image;

	// Compute the value for a single luxel in the lightmap
	// The light queue holds relative luxel ranges in the light_queue_offs
	// array with the lightmap subpic id in the corresponding entry in
	// the light_queue_inds array.
	// per luxel, indirect dispatch (light_queue)
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void main ()
	{
		uint luxelid = gl_GlobalInvocationID.x;
		if (luxelid >= light_queue[2].count) {
			return;
		}
		uint ind = fbsearch (luxelid, light_queue_offs, light_queue[0].count);
		uint luxel = luxelid - light_queue_offs[ind];
		auto li = &lightinfo[light_queue_inds[ind]];
		if (!li.size.x || !li.size.y) {
			// no subpic was allocated
			printf ("unallocated subpic: %d\n", light_queue_inds[ind]);
			return;
		}
		auto xy = ivec2 (luxel % li.size.x, luxel / li.size.x) + ivec2 (li.pos);
		auto light = vec3 (0);
		if (li.data == ~0u) {
			light = vec3 (1);
		} else if (li.data == 0xdeadbeef) {
			light = vec3 (1, 0, 1);
		} else {
			auto _data = &lightmap_data[li.data];
			// lightmap data is rgb
			auto data = (ubvec3*)_data;
			uint stride = li.size.x * li.size.y;
			// style 0xff marks the premature end of the array
			for (uint i = 0; i < 4; i++) {
				uint style = li.styles[i];
				if (style != 255) {
					float scale = light_style_values[style] / 65536.0f;
					light += vec3 (data[i * stride + luxel]) * scale;
				}
			}
		}
		imageStore (light_image, xy, vec4 (light, 1));
	}

	// Add (luxel count, lightmap id) pairs per surface in each cluster to
	// the light_queue. The luxel count is written to light_queue_offs to
	// later be prefix summed and the id is written to light_queue_inds per
	// cluster, indirect dispatch (cluster_queue)
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void queue_surfs ()
	{
		uint ind = gl_GlobalInvocationID.x;
		if (ind >= cluster_queue.count) {
			return;
		}
		uint clusterid = cluster_queue.queue[ind];
		light_cluster_surfs[ind] = light_clusters[clusterid].count;
		if (ind == 0) {
			light_queue[0].count = cluster_queue.count;
		}
	}

	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void luxels ()
	{
		uint surfid = gl_GlobalInvocationID.x;
		if (surfid >= light_queue[3].count) {
			return;
		}
		uint ind = fbsearch (surfid, light_cluster_surfs, cluster_queue.count);
		uint clusterid = cluster_queue.queue[ind];
		auto cluster = light_clusters[clusterid];
		//FIXME don't queue all visible lightmaps, only those that need to be
		//updated (need to cache previous style values)
		uint surf = cluster.first + surfid - light_cluster_surfs[ind];
		auto li = &lightinfo[surf];
		light_queue_offs[surfid] = (uint) li.size.x * (uint) li.size.y;
		light_queue_inds[surfid] = surf;
		if (surfid == 0) {
			light_queue[0].count = light_queue[3].count;
		}
	}

	[shader(GLCompute, LocalSize=[1,1,1])]
	void clear ()
	{
		if (light_queue) {
			light_queue[0].count = 0;	// prefixsum main+offset
			light_queue[0].x = 0;
			light_queue[1].count = 0;	// prefixsum sums
			light_queue[1].x = 0;
			light_queue[2].count = 0;	// per luxel update
			light_queue[2].x = 0;
			light_queue[3].count = 0;
			light_queue[3].x = 0;
		}
	}

	@namespace set_dispatch {
		[shader(GLCompute, LocalSize=[1,1,1])]
		void cluster ()
		{
			if (cluster_queue) {
				cluster_queue.x = DISPATCH(cluster_queue.count);
				//printf ("set_dispatch.cluster: %d\n", cluster_queue.count);
			}
		}

		[shader(GLCompute, LocalSize=[1,1,1])]
		void luxel ()
		{
			if (light_queue) {
				uint count = light_queue[3].count;
				light_queue[3].x = DISPATCH(count);
				light_queue[3].y = 1;
				light_queue[3].z = 1;
				//printf ("set_dispatch.luxel: %d\n", count);
			}
		}

		[shader(GLCompute, LocalSize=[1,1,1])]
		void surf ()
		{
			if (light_queue) {
				uint count = light_queue[0].count;
				uint sum_count = BLOCKDISP (count);
				light_queue[0].x = BLOCKDISP(count);
				light_queue[0].y = 1;
				light_queue[0].z = 1;
				light_queue[1].count = sum_count;
				//FIXME light_queue[1].x = BLOCKDISP(BLOCKDISP (count));
				light_queue[1].x = BLOCKDISP(sum_count);
				light_queue[1].y = 1;
				light_queue[1].z = 1;

				prefixsum_counts[0] = count;
				prefixsum_counts[1] = sum_count;
				//printf ("set_dispatch.surf:%d %d\n", count, sum_count);
			}
		}

		[shader(GLCompute, LocalSize=[1,1,1])]
		void update ()
		{
			if (light_queue) {
				uint count = light_queue[2].count;
				light_queue[2].x = DISPATCH(count);
				light_queue[2].y = 1;
				light_queue[2].z = 1;
				//printf ("set_dispatch.update %d\n", count);
			}
		}
	}
}
