#include <GLSL/image.h>

#include "fbsearch.r"

#define workgroup_size 512
#define DISPATCH(x) (((x) + workgroup_size - 1) / workgroup_size)
#define BLOCKDISP(x) (((x) + (2 * workgroup_size) - 1) / (2 * workgroup_size))

void printf (string fmt, ...)
	= @intrinsic(OpExtInst, "NonSemantic.DebugPrintf", DebugPrintf);

[in("GlobalInvocationId")] uvec3 gl_GlobalInvocationID;

typedef struct bsp_invoke_s {
	uint        count;
	uint        x, y, z;
} bsp_invoke_t;

typedef struct scrapbox_s {
	ushort      x, y, layer;
	ushort      width, height;
	ushort      scrap_id;   // which scrap
	uint        id;         // scrap internal id
} scrapbox_t;

typedef struct {
	vec2        offset;
	vec2        uv;
} quadvert_t;


[push_constant] @block Params {
	bsp_invoke_t *texel_queue;
	uint       *texel_offs;
	byte       *texel_data;
	quadvert_t *vertex_data;
	scrapbox_t *rect_data;
	uint        format;
	uint        num_rects;
	uvec2       extents;

	uint       *prefixsum_counts;
	bsp_invoke_t *atlas_invoke;
};

uvec3 get_pos (uint texel_id)
{
	uint ind = fbsearch (texel_id, texel_offs, num_rects);
	uint texel = texel_id - texel_offs[ind];
	auto r = &rect_data[ind];
	auto pos = ivec3 (r.x + texel % r.width, r.y + texel / r.width, r.layer);
	return pos;
}

@namespace tex_l {
	[uniform, set(0), binding(0)]
	@image(float, 2D, Array, Storage, R8) atlas_image;

	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void main ()
	{
		uint texel_id = gl_GlobalInvocationID.x;
		if (texel_id >= texel_queue.count) {
			return;
		}
		auto pos = get_pos (texel_id);
		//FIXME qfcc doesn't cast for imageStore
		auto data = float (((byte *) texel_data)[texel_id]) / 255;
		imageStore (atlas_image, pos, data);
	}
}

@namespace tex_a {
	[uniform, set(0), binding(0)]
	@image(float, 2D, Array, Storage, R8) atlas_image;

	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void main ()
	{
		uint texel_id = gl_GlobalInvocationID.x;
		if (texel_id >= texel_queue.count) {
			return;
		}
		auto pos = get_pos (texel_id);
		//FIXME qfcc doesn't cast for imageStore
		auto data = float (((byte *) texel_data)[texel_id]) / 255;
		imageStore (atlas_image, pos, data);
	}
}

@namespace tex_la {
	[uniform, set(0), binding(0)]
	@image(float, 2D, Array, Storage, Rg8) atlas_image;

	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void main ()
	{
		uint texel_id = gl_GlobalInvocationID.x;
		if (texel_id >= texel_queue.count) {
			return;
		}
		auto pos = get_pos (texel_id);
		//FIXME qfcc doesn't cast for imageStore
		auto data = vec2 (((ubvec2 *) texel_data)[texel_id]) / 255;
		imageStore (atlas_image, pos, data);
	}
}

@namespace tex_rgb {
	[uniform, set(0), binding(0)]
	@image(float, 2D, Array, Storage, Rgba8) atlas_image;

	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void main ()
	{
		uint texel_id = gl_GlobalInvocationID.x;
		if (texel_id >= texel_queue.count) {
			return;
		}
		auto pos = get_pos (texel_id);
		//FIXME qfcc doesn't cast for imageStore
		auto data = vec4 (((ubvec3 *) texel_data)[texel_id], 255) / 255;
		imageStore (atlas_image, pos, data);
	}
}

@namespace tex_rgba {
	[uniform, set(0), binding(0)]
	@image(float, 2D, Array, Storage, Rgba8) atlas_image;

	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void main ()
	{
		uint texel_id = gl_GlobalInvocationID.x;
		if (texel_id >= texel_queue.count) {
			return;
		}
		auto pos = get_pos (texel_id);
		//FIXME qfcc doesn't cast for imageStore
		auto data = vec4 (((ubvec4 *) texel_data)[texel_id]) / 255;
		imageStore (atlas_image, pos, data);
	}
}

@namespace tex_frgba {
	[uniform, set(0), binding(0)]
	@image(float, 2D, Array, Storage, Rgba32f) atlas_image;

	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void main ()
	{
		uint texel_id = gl_GlobalInvocationID.x;
		if (texel_id >= texel_queue.count) {
			return;
		}
		auto pos = get_pos (texel_id);
		//FIXME qfcc doesn't cast for imageStore
		auto data = ((vec4 *) texel_data)[texel_id];
		imageStore (atlas_image, pos, data);
	}
}

@namespace rects {
	[shader(GLCompute, LocalSize=[workgroup_size,1,1])]
	void main ()
	{
		uint ind = gl_GlobalInvocationID.x;
		if (ind >= num_rects) {
			return;
		}
		auto r = &rect_data[ind];
		auto verts = vertex_data + 4 * r.id;
		float x = 0;
		float y = 0;
		float w = r.width;
		float h = r.height;
		float u = r.x;
		float v = r.y;
		//FIXME qfcc does bit cast instead of value cast for implicit
		auto s = vec2 (1f) / vec2(extents);
		verts[0] = {
			.offset = vec2 (x, y),
			.uv     = vec2 (u, v) * s,
		};
		verts[1] = {
			.offset = vec2 (x, y + h),
			.uv     = vec2 (u, v + h) * s,
		};
		verts[2] = {
			.offset = vec2 (x + w, y),
			.uv     = vec2 (u + w, v) * s,
		};
		verts[3] = {
			.offset = vec2 (x + w, y + h),
			.uv     = vec2 (u + w, v + h) * s,
		};
		texel_offs[ind] = r.width * r.height;
	}
}

@namespace set_dispatch {
	[shader(GLCompute, LocalSize=[1,1,1])]
	void texel ()
	{
		uint count = texel_queue.count;
		texel_queue.x = DISPATCH(count);
		texel_queue.y = 1;
		texel_queue.z = 1;
	}

	[shader(GLCompute, LocalSize=[1,1,1])]
	void rect ()
	{
		uint count = num_rects;
		uint sum_count = BLOCKDISP (count);
		atlas_invoke[0] = {
			.count = count,
			.x = BLOCKDISP (count),
			.y = 1,
			.z = 1,
		};
		atlas_invoke[1] = {
			.count = sum_count,
			.x = BLOCKDISP (sum_count),
			.y = 1,
			.z = 1,
		};
		prefixsum_counts[0] = count;
		prefixsum_counts[1] = sum_count;
	}
}
