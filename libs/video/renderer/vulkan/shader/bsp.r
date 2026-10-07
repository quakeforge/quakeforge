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

typedef struct subcluster_queue_s {
	uint        count;
	uint        x, y, z;
	uint        queue[];
} subcluster_queue_t;

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

@namespace cluster {
	[push_constant] @block Params {
		uint       *command_counts;
		uint       *command_offsets;
		bsp_command_t *commands;
		bsp_cluster_t *subclusters;
		cluster_t  *clusters;
		uint       *cluster_map;
		subcluster_queue_t *subcluster_queue;
		uint       *subcluster_tmp;
		instance_queue_t *instance_queue;
		uint        texture_count;
		uint        anim_index;

		bsp_model_t *models;

		bsp_texanim_t *anim_main;	///< group 0 animations
		bsp_texanim_t *anim_alt;	///< group 1 animations
		ushort     *frame_map;		///< map from texture frame to texture id

		bsp_invoke_t *mod_invoke;

		uint       *prefixsum_counts;
	};

	uint map_tex_id (uint tex_id, uint frame, uint trans)
	{
		auto frame_anim = frame ? anim_alt : anim_main;
		trans *= texture_count;
		auto anim = frame_anim[tex_id];
		uint anim_ind = (anim_index + anim.offset) % anim.count;
		return frame_map[anim.base + anim_ind] + trans;
	}

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
	[capability(GroupNonUniformArithmetic)]
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void queue_textures ()
	{
		uint sc_id = gl_GlobalInvocationID.x;
		if (sc_id >= subcluster_queue.count) {
			return;
		}
		uint count = instance_queue.count;
		//FIXME (qfcc) passing array[] to ptr
		uint ind = fbsearch (sc_id, &subcluster_queue.queue[0], count);
		auto queue = &instance_queue.queue[ind];
		auto cluster = &clusters[queue.cluster];
		uint sc_ind = cluster.first + sc_id - subcluster_queue.queue[ind];
		auto sc = &subclusters[cluster_map[sc_ind]];
		uint tex_id = map_tex_id (sc.tex_id, queue.frame, queue.trans);

		uint command_ind = subcluster_tmp[sc_id];
		command_ind += command_offsets[tex_id];
		commands[command_ind] = (bsp_command_t) {
			.indexCount = sc.index_count,
			.instanceCount = queue.instance_count,
			.firstIndex = sc.first_index,
			.vertexOffset = 0,
			.firstInstace = queue.first_instance,
		};
	}

	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void alloc_textures ()
	{
		uint sc_id = gl_GlobalInvocationID.x;
		if (sc_id >= subcluster_queue.count) {
			return;
		}
		uint count = instance_queue.count;
		//FIXME (qfcc) passing array[] to ptr
		uint ind = fbsearch (sc_id, &subcluster_queue.queue[0], count);
		auto queue = &instance_queue.queue[ind];
		auto cluster = &clusters[queue.cluster];
		uint sc_ind = cluster.first + sc_id - subcluster_queue.queue[ind];
		auto sc = &subclusters[cluster_map[sc_ind]];
		uint tex_id = map_tex_id (sc.tex_id, queue.frame, queue.trans);
		subcluster_tmp[sc_id] = atomicAdd (command_counts[tex_id], 1);
	}

	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void count_subclusters ()
	{
		uint ind = gl_GlobalInvocationID.x;
		if (ind >= instance_queue.count) {
			return;
		}
		uint cluster_ind = instance_queue.queue[ind].cluster;
		subcluster_queue.queue[ind] = clusters[cluster_ind].count;
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

	@namespace set_dispatch {
		[shader(GLCompute, LocalSize=[1,1,1])]
		void subcluster ()
		{
			if (mod_invoke) {
				uint count = instance_queue.count;
				uint sum_count = BLOCKDISP (count);
				mod_invoke[0] = {
					.count = count,
					.x = BLOCKDISP(count),
					.y = 1,
					.z = 1,
				};
				mod_invoke[1] = {
					.count = sum_count,
					.x = BLOCKDISP(sum_count),
					.y = 1,
					.z = 1,
				};
				prefixsum_counts[0] = count;
				prefixsum_counts[1] = sum_count;
			}
		}

		[shader(GLCompute, LocalSize=[1,1,1])]
		void texture ()
		{
			if (subcluster_queue) {
				uint count = subcluster_queue.count;
				subcluster_queue.x = DISPATCH(count);
				subcluster_queue.y = 1;
				subcluster_queue.z = 1;
			}
		}
	}
}

@namespace ent {
	[push_constant] @block Params {
		uint       *ent_ids;
		uint       *ent_rel;
		uint       *inst_ids;
		Entity     *entities;
		instance_queue_t *instance_queue;
		cluster_queue_t *cluster_queue;
		bsp_model_t *models;

		uint       *mod_counts;
		uint       *mod_offsets;
		uint       *mod_clusters;
		uint        ent_count;
		uint        num_models;
		bsp_invoke_t *mod_invoke;

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

		uint inst_ind = mod_offsets[mod_base + mod_id] + ent_rel[ent_id];
		inst_ids[inst_ind] = ent_id;
	}

	// Count the clusters for all the models except world.
	// The world model (0) was handled by world_instance() as its clusters
	// are controlled by the PVS for the camera cluster.
	// per model*queue, direct dispatch
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void model_count ()
	{
		uint queue_mod_id = gl_GlobalInvocationID.x;
		if (queue_mod_id == 0) {
			// copy world model cluster count so it offsets the model clusters
			// when prefix-summed
			mod_clusters[0] = instance_queue.count;
		}
		uint mod_id = queue_mod_id % num_models;
		if (mod_id < 1 || queue_mod_id >= num_models * mod_queues) {
			return;
		}
		auto mod = models[mod_id];
		bool vis = mod_counts[queue_mod_id] > 0;
		mod_clusters[queue_mod_id] = vis ? mod.cluster_count : 0;
	}

	// Set up the instance queue entries for all the models except world (see
	// model_count).
	// per cluster, indirect dispatch (cluster_queue)
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void model_instances ()
	{
		uint cluster_id = gl_GlobalInvocationID.x;
		if (cluster_id == 0) {
			instance_queue.count = cluster_queue.count;
		}
		// skip world model clusters
		if (cluster_id < instance_queue.count
			|| cluster_id >= cluster_queue.count) {
			return;
		}
		uint count = num_models * mod_queues;
		uint queue_mod_id = fbsearch (cluster_id, mod_clusters, count);
		uint first_instance = mod_offsets[queue_mod_id];
		uint instance_count = mod_counts[queue_mod_id];

		uint rel_cluster = cluster_id - mod_clusters[queue_mod_id];
		uint queue = queue_mod_id / num_models;
		uint mod_id = queue_mod_id % num_models;
		auto mod = &models[mod_id];
		uint cluster = mod.first_cluster + rel_cluster;
		bsp_queue_t q = {
			.cluster = cluster,
			.first_instance = first_instance,
			.instance_count = instance_count,
			//FIXME qfcc doesn't warn (produces bad spir-v
			//without cast)
			.frame = (ushort) (queue & 1),
			.trans = (ushort) ((queue >> 1) & 1),
		};
		instance_queue.queue[cluster_id] = q;
		cluster_queue.queue[cluster_id] = cluster;
	}

	// Count the entity instances on each model
	// per entity, direct dispatch
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void count ()
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

		uint offs = atomicAdd (mod_counts[mod_base + mod_id], 1);
		ent_rel[ent_id] = offs;
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

	// Reset the model instance and cluster counts
	// per model*queue, direct dispatch
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void clear ()
	{
		uint mod_id = gl_GlobalInvocationID.x;
		if (mod_id < num_models * mod_queues) {
			mod_counts[mod_id] = 0;
			mod_clusters[mod_id] = 0;
		}
	}

	@namespace set_dispatch {
		[shader(GLCompute, LocalSize=[1,1,1])]
		void instance ()
		{
			if (instance_queue) {
				instance_queue.x = DISPATCH(instance_queue.count);
				instance_queue.y = 1;
				instance_queue.z = 1;
			}
		}

		[shader(GLCompute, LocalSize=[1,1,1])]
		void model ()
		{
			if (mod_invoke) {
				uint count = num_models * mod_queues;
				uint sum_count = BLOCKDISP (count);
				mod_invoke[0] = {
					.count = count,
					.x = BLOCKDISP(count),
					.y = 1,
					.z = 1,
				};
				mod_invoke[1] = {
					.count = sum_count,
					.x = BLOCKDISP(sum_count),
					.y = 1,
					.z = 1,
				};
				prefixsum_counts[0] = count;
				prefixsum_counts[1] = sum_count;
			}
		}

		[shader(GLCompute, LocalSize=[1,1,1])]
		void cluster ()
		{
			if (cluster_queue) {
				cluster_queue.x = DISPATCH(cluster_queue.count);
				cluster_queue.y = 1;
				cluster_queue.z = 1;
			}
		}
	}
}

@namespace lightmap {
	[push_constant] @block Params {
		bsp_lightinfo_t *lightinfo;
		bsp_surfinfo_t *surfinfo;
		bsp_lightcache_t *light_cache;
		short      *light_style_values;
		byte       *lightmap_data;
		uint       *light_queue_offs;
		uint       *light_queue_inds;
		cluster_t  *light_clusters;
		uint       *light_cluster_surfs;
		cluster_queue_t *cluster_queue;
		bsp_invoke_t *light_queue;

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
		auto vals = svec4 (
			light_style_values[li.styles[0]],
			light_style_values[li.styles[1]],
			light_style_values[li.styles[2]],
			light_style_values[li.styles[3]]
		);
		auto cached = svec4 (
			light_cache[surf].vals[0],
			light_cache[surf].vals[1],
			light_cache[surf].vals[2],
			light_cache[surf].vals[3]
		);
		bool changed = @horiz (| vals != cached);
		light_cache[surf].vals = {vals[0], vals[1], vals[2], vals[3]};
		uint size = changed ? (uint) li.size.x * (uint) li.size.y : 0;
		light_queue_offs[surfid] = size;
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
