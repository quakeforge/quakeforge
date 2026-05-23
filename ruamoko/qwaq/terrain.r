#include <QF/qfmodel.h>

#include <GLSL/atomic.h>
#include <GLSL/general.h>

#include "terrain.h"

//void printf(string fmt, ...)=#0;

#define WORKGROUP_SIZE 64

[push_constant] @block PushConstants {
	queue_t  **update_queues;
	MeshData  *mesh_data;
	CameraData *camera_data;
	NeighborInds neighbor_inds;
	float       triangle_size;
	uint        reduce_level;
	uint        max_depth;
};

[in("GlobalInvocationId")]
uvec3 GlobalInvocationId;
#define threadID GlobalInvocationId.x

@namespace dispatch {

[shader(GLCompute, LocalSize=[1,1,1])]
[capability(Int64)]
void
prepare ()
{
	mesh_data.indirect[threadID].x = update_queues[threadID].count;
}
}

@namespace cbt {
#define group_levels 5
#define bits_size (1u << (mesh_data.cbt.depth - group_levels))
#define group_base bits_size
#define cbt_size (group_base + group_levels * bits_size + bits_size)
#define cbt_levels (mesh_data.cbt.depth - group_levels)
#define bits_base (group_base + group_levels * bits_size)

uint size ()
{
	return mesh_data.cbt.heap[0];
}

uint count ()
{
	return mesh_data.cbt.heap[1];
}

uint decode_bit (uint index)
{
	auto heap = mesh_data.cbt.heap;
	if (index >= heap[1]) {
		return -1;
	}
	uint bitID = 1;
	while (bitID < (1u << cbt_levels)) {
		bitID <<= 1;
		uint count = heap[bitID];
		if (index >= count) {
			index -= count;
			bitID += 1;
		}
	}
	uint count_mask = 0xffff;
	uint ind_mask = 1;
	uint shift = 16;
	uint ind = bitID + bits_size;
	bitID = (bitID - group_base) * 1;
	while (ind < cbt_size) {
		bitID <<= 1;
		uint bs = shift * (bitID & ind_mask);
		uint count = (heap[ind] >> bs) & count_mask;
		shift >>= 1;
		count_mask >>= shift;
		ind_mask = (ind_mask << 1) + 1;
		ind += bits_size;
		if (index >= count) {
			index -= count;
			bitID += 1;
		}
	}
	return bitID;
}

uint decode_bit_complement (uint index)
{
	auto heap = mesh_data.cbt.heap;
	if (index >= heap[0] - heap[1]) {
		return -1;
	}
	uint bitID = 1;
	uint c = 1 << (mesh_data.cbt.depth - 1);
	while (bitID < (1u << cbt_levels)) {
		bitID <<= 1;
		if (index >= c - heap[bitID]) {
			index -= c - heap[bitID];
			bitID += 1;
		}
		c >>= 1;
	}
	uint count_mask = 0xffff;
	uint ind_mask = 1;
	uint shift = 16;
	uint ind = bitID + bits_size;
	bitID = (bitID - group_base) * 1;
	while (ind < cbt_size) {
		bitID <<= 1;
		uint bs = shift * (bitID & ind_mask);
		uint count = c - ((heap[ind] >> bs) & count_mask);
		shift >>= 1;
		count_mask >>= shift;
		ind_mask = (ind_mask << 1) + 1;
		ind += bits_size;
		if (index >= count) {
			index -= count;
			bitID += 1;
		}
		c >>= 1;
	}
	return bitID;
}

void set_bit_buffer (const uint bitID, const uint val)
{
	auto heap = mesh_data.cbt.heap;
	uint ind = bits_base + (bitID >> group_levels);
	uint mask = 1 << (bitID & ((1 << group_levels) - 1));

	if (val) {
		atomicOr (heap[ind], mask);
	} else {
		atomicAnd (heap[ind], ~mask);
	}
}

void first_pass()
{
	auto heap = mesh_data.cbt.heap;
	uint ind = threadID + group_base;
	uint bits = heap[ind + 5 * bits_size];
	bits = (bits & 0x55555555) + ((bits >>  1) & 0x55555555);
	heap[ind + 4 * bits_size] = bits;
	bits = (bits & 0x33333333) + ((bits >>  2) & 0x33333333);
	heap[ind + 3 * bits_size] = bits;
	bits = (bits & 0x0f0f0f0f) + ((bits >>  4) & 0x0f0f0f0f);
	heap[ind + 2 * bits_size] = bits;
	bits = (bits & 0x00ff00ff) + ((bits >>  8) & 0x00ff00ff);
	heap[ind + 1 * bits_size] = bits;
	bits = (bits & 0x0000ffff) + ((bits >> 16) & 0x0000ffff);
	heap[ind + 0 * bits_size] = bits;
}

uint reduce_level;

void second_pass()
{
	auto heap = mesh_data.cbt.heap;
	uint ind = threadID + (1 << reduce_level);
	heap[ind] = heap[ind * 2] + heap[ind * 2 + 1];
}

}

//@overload
//uint findMSB(ulong x)
//{
//	auto ul = @bitcast (uvec2, x);
//	if (ul[1]) {
//		return findMSB (ul[1]);
//	} else {
//		return findMSB (ul[0]);
//	}
//}

uint countbits (uint x)
{
	uint c = 0;
	for (; x; c++) {
		x &= x - 1;
	}
	return c;
}

uint
heap_depth (const ulong x)
{
	//return findMSB (x);
	auto ul = @bitcast (uvec2, x);
	if (ul[1]) {
		return findMSB (ul[1]);
	} else {
		return findMSB (ul[0]);
	}
}

void
queue_id (queue_t *queue, const uint id)
{
	uint slot = atomicAdd (queue.count, 1);
	queue.entries[slot] = id;
}

bool
reserve_memory (const int amount)
{
	int old = atomicAdd (mesh_data.memory.available, -amount);
	if (old < amount) {
		atomicAdd (mesh_data.memory.available, (uint) amount);
		return false;
	}
	return true;
}

uint
allocate_memory (const int amount)
{
	return atomicAdd (mesh_data.memory.allocated, (uint) amount);
}

bool
set_subdiv (const uint bisector, const uint subdiv)
{
	uint old = atomicOr (mesh_data.bisector_data[bisector].subdiv, subdiv);
	return old == 0;
}

enum {
	back_face_culled = -3,
	frustum_culled,
	too_small,
	unchanged_element,
	bisect_element,
	simplify_element,
	merged_element,
};

enum {
	center_split = 1,
	right_split = 2,
	left_split = 4
};

@namespace draw.update {
mat3
RootBisectorVertices(const uint halfedgeID)
{
	auto halfedges = mesh_data.base.halfedges;
	auto vertices = mesh_data.base.vertices;
	uint nextID = halfedges[halfedgeID].next;
	vec3 v0 = vertices[halfedges[halfedgeID].vert];
	vec3 v1 = vertices[halfedges[nextID].vert];
	vec3 v2 = v0;
	uint n = 1;
	uint h = nextID;
	while (h != halfedgeID) {
		v2 = v2 + vertices[halfedges[h].vert];
		n += 1;
		h = halfedges[h].next;
	}
	v2 /= n;
	return mat3(v0, v1, v2);
}

mat3
BisectorVertices (const ulong heapID)
{
	static const mat3 Mb[] = {
		{ { 0.0, 0.0, 1.0 },
		  { 1.0, 0.0, 0.0 },
		  { 0.5, 0.5, 0.0 } },
		{ { 0.0, 1.0, 0.0 },
		  { 0.0, 0.0, 1.0 },
		  { 0.5, 0.5, 0.0 } },
	};
	auto M = mat3(1);
	ulong h = heapID;
	ulong base = 1 << mesh_data.base.depth;
	while (h >= base << 1) {
		uint b = (uint) h & 1;
		M = Mb[b] * M;
		h >>= 1;
	}
	h &= base - 1;
	return RootBisectorVertices (h) * M;
}

[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	auto draw = &mesh_data.draw;
	if (threadID >= draw.indirect[0].vertexCount / 3) {
		return;
	}
	uint selfID = cbt.decode_bit (threadID);
	ulong heapID = mesh_data.bisector_ids[selfID];
	auto v = BisectorVertices (heapID);
	draw.vertices[3 * selfID + 0] = v[0];
	draw.vertices[3 * selfID + 1] = v[1];
	draw.vertices[3 * selfID + 2] = v[2];
}
}

@namespace reset {
[shader(GLCompute, LocalSize=[1,1,1])]
[capability(Int64)]
void
main ()
{
	mesh_data.memory = {
		.allocated = 0,
		.available = cbt.size() - cbt.count(),
	};
	mesh_data.queues.split.count = 0;
	mesh_data.queues.simplify.count = 0;
	mesh_data.queues.allocate.count = 0;
	mesh_data.queues.propagate[0].count = 0;
	mesh_data.queues.propagate[1].count = 0;
	mesh_data.queues.simplification.count = 0;
	mesh_data.draw.indirect = {
		{ 0, 1, 0, 0 },
		{ 0, 1, 0, 0 },
	};
	mesh_data.draw.modified_bisector_count = 0;
}
}

@namespace classify {

bool
frustum_aabb_intersect (const vec4 aabb_min, const vec4 aabb_max)
{
	auto center = (aabb_max + aabb_min) * 0.5;
	auto extents = (aabb_max - aabb_min) * 0.5;
	for (int i = 0; i < countof (camera_data.frustum_planes); i++) {
		auto plane = camera_data.frustum_planes[i];
		auto tp = center + sign (plane) * extents;
		if (tp • plane < 0) {
			return false;
		}
	}
	return true;
}


int
classify_element (const uint selfID, const uint depth)
{
	auto draw = &mesh_data.draw;
	vec3 points[4] = {
		draw.vertices[3 * selfID + 0],
		draw.vertices[3 * selfID + 1],
		draw.vertices[3 * selfID + 2],
		draw.vertices[3 * mesh_data.cbt.max_count + selfID],
	};

	vec3 n = normalize((points[1] - points[0]) × (points[2] - points[0]));
	vec3 c = (points[0] + points[1] + points[2]) / 3;
	vec3 vd = normalize (camera_data.camera_pos - c);
	float fv = -vd • camera_data.camera_fwd;
	float vn = vd • n;

	if (fv < 0 || vn < -1e-3) {
		return back_face_culled;
	}

	vec4 aabbMin = [min (points[0], min (points[1], points[2])), 1];
	vec4 aabbMax = [max (points[0], max (points[1], points[2])), 1];
	if (!frustum_aabb_intersect (aabbMin, aabbMax)) {
		return frustum_culled;
	}

	vec4 p0 = camera_data.ProjView * [points[0], 1];
	vec4 p1 = camera_data.ProjView * [points[1], 1];
	vec4 p2 = camera_data.ProjView * [points[2], 1];
	p0 /= p0.w;
	p1 /= p1.w;
	p2 /= p2.w;
	vec2 pa = p1.xy - p0.xy;
	vec2 pb = p2.xy - p0.xy;
	float area = (pa.x * pb.y - pa.y * pb.x) * -0.5;

	if (area > triangle_size && depth < max_depth) {
		return bisect_element;
	}
	if (area < triangle_size * 0.5 || depth > max_depth) {
		vec4 p3 = camera_data.ProjView * [points[3], 1];
		p3 /= p3.w;
		vec2 pc = p3.xy - p0.xy;
		float parent_area = (pa.x * pc.y - pa.y * pc.x) * -0.5;
		if (area < triangle_size || depth > max_depth) {
			return too_small;
		}
	}
	return unchanged_element;
}

[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	auto draw = &mesh_data.draw;
	if (threadID > draw.bisector_count) {
		return;
	}
	uint selfID = mesh_data.bisector_map[threadID];
	uint depth = heap_depth (mesh_data.bisector_ids[selfID]);
	int valid = classify_element (selfID, depth);
	auto self = mesh_data.bisector_data[selfID];
	self.subdiv = 0;
	self.modified = 0;
	self.visible = 0;
	if (valid > unchanged_element) {
		self.state = bisect_element;
		queue_id (&mesh_data.queues.split, selfID);
	}
	self.visible = valid >= too_small;
	if (valid < unchanged_element && depth != mesh_data.base.depth) {
		self.state = simplify_element;

		ulong heapID = mesh_data.bisector_ids[selfID];
		// Register only even heapIDs as odd ones will be taken care of by
		// the even ones
		if (!(heapID & 1)) {
			queue_id (&mesh_data.queues.simplify, selfID);
		}
	}
	mesh_data.bisector_data[selfID] = self;
}
}

@namespace split {
[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	if (threadID > mesh_data.queues.split.count) {
		return;
	}
	uint selfID = mesh_data.queues.split.entries[threadID];
	auto neighbors = mesh_data.neighbors[neighbor_inds.cur];
	auto n = neighbors[selfID];
	if (n.prev != ~0u) {
		// on path
		if (neighbors[n.prev].twin == selfID
			&& mesh_data.bisector_data[n.prev].state != unchanged_element) {
			return;
		}
	}
	if (n.next != ~0u) {
		// on path
		if (neighbors[n.next].twin == selfID
			&& mesh_data.bisector_data[n.next].state != unchanged_element) {
			return;
		}
	}
	ulong heapID = mesh_data.bisector_ids[selfID];
	uint depth = heap_depth (heapID);
	int max_memory = 2 * (depth - mesh_data.base.depth) - 1;
	if (n.twin == ~0u) {
		// on the edge of the mesh
		max_memory = 1;
	} else if (neighbors[n.twin].twin == selfID) {
		max_memory = 2;
	}
	if (!reserve_memory (max_memory)) {
		return;
	}
	if (!set_subdiv (selfID, center_split)) {
		// other neighbor beat us to it, return reserved memory
		reserve_memory (-max_memory);
		return;
	}

	queue_id (&mesh_data.queues.allocate, selfID);

	uint twinID = n.twin;
	uint used_memory = 1;
	bool done = false;
	while (!done) {
		if (twinID == ~0u) {
			break;
		}
		uint t_depth = heap_depth (mesh_data.bisector_ids[twinID]);
		if (t_depth == depth) {
			// both triangles are at the same detph
			if (set_subdiv (twinID, center_split)) {
				queue_id (&mesh_data.queues.allocate, twinID);
			}
			done = true;
		} else {
			auto tn = neighbors[twinID];
			// either twin.prev or twin.next == selfID
			uint subdiv = neighbors[twinID].prev == selfID
				? center_split | right_split
				: center_split | left_split;
			if (!set_subdiv (twinID, subdiv)) {
				used_memory++;
				done = true;
			} else {
				queue_id (&mesh_data.queues.allocate, twinID);
				// need two splits
				used_memory += 2;
				selfID = twinID;
				depth = t_depth;
				twinID = neighbors[selfID].twin;
			}
		}
	}
	reserve_memory (used_memory - max_memory);
}
}

@namespace allocate {
[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	if (threadID > mesh_data.queues.allocate.count) {
		return;
	}
	uint selfID = mesh_data.queues.allocate.entries[threadID];
	auto bisector = mesh_data.bisector_data[selfID];
	if (bisector.subdiv) {
		uint count = countbits (bisector.subdiv);
		uint first = allocate_memory (count);
		for (uint i = 0; i < count; i++) {
			uint index = cbt.decode_bit_complement (first + i);
			bisector.indices[i] = index;
		}
		mesh_data.bisector_data[selfID] = bisector;
	}
}
}

@namespace bisect {
void
evaluate_neighbors (const uint selfID, const uint twinID,
				    @out uint resX, @out uint resY)
{
	auto twin = mesh_data.bisector_data[twinID];
	auto neighbors = mesh_data.neighbors[neighbor_inds.cur];
	auto n = neighbors[twinID];
	uint subdiv = twin.subdiv;
	if (subdiv == center_split) {
		resX = twin.indices[0];
		resY = twinID;
	} else if (subdiv == (center_split|right_split)) {
		if (n.prev == selfID) {
			resX = twin.indices[1];
			resY = twinID;
		} else {
			resX = twin.indices[0];
			resY = twin.indices[1];
		}
	} else if (subdiv == (center_split|left_split)) {
		if (n.next == selfID) {
			resX = twin.indices[1];
			resY = twin.indices[0];
		} else {
			resX = twin.indices[0];
			resY = twinID;
		}
	} else {
		if (n.prev == selfID) {
			resX = twin.indices[1];
			resY = twinID;
		} else if (n.next == selfID) {
			resX = twin.indices[2];
			resY = twin.indices[0];
		} else {
			resX = twin.indices[0];
			resY = twin.indices[1];
		}
	}
}

[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	if (threadID > mesh_data.queues.allocate.count) {
		return;
	}
	uint selfID = mesh_data.queues.allocate.entries[threadID];
	ulong heapID = mesh_data.bisector_ids[selfID];
	auto bisector = mesh_data.bisector_data[selfID];
	auto in_neighbors = mesh_data.neighbors[neighbor_inds.cur];
	auto out_neighbors = mesh_data.neighbors[neighbor_inds.next];
	auto n = in_neighbors[selfID];
	uint subdiv = bisector.subdiv;
	uint sib0ID = bisector.indices[0];
	uint sib1ID = bisector.indices[1];
	uint sib2ID = bisector.indices[2];
	if (subdiv == center_split) {
		uint resX = ~0u, resY = ~0u;
		if (n.twin != ~0u) {
			evaluate_neighbors (selfID, n.twin, resX, resY);
		}

		mesh_data.bisector_ids[selfID] = 2 * heapID;
		mesh_data.bisector_ids[sib0ID] = 2 * heapID + 1;
		out_neighbors[selfID] = {sib0ID, resX, n.prev};
		out_neighbors[sib0ID] = {resY, selfID, n.next};

		auto b = bisector;
		b.propagationID = selfID;
		b.modified = true;

		b.problem_neighbor = ~0u;
		mesh_data.bisector_data[selfID] = b;

		b.problem_neighbor = n.next;
		mesh_data.bisector_data[sib0ID] = b;

		queue_id (&mesh_data.queues.propagate[0], sib0ID);
	} else if (subdiv == (center_split|right_split)) {
		uint res0X, res0Y;
		uint res1X = ~0u, res1Y = ~0u;
		evaluate_neighbors (selfID, n.prev, res0X, res0Y);
		if (n.twin != ~0u) {
			evaluate_neighbors (selfID, n.twin, res1X, res1Y);
		}

		mesh_data.bisector_ids[selfID] = 4 * heapID;
		mesh_data.bisector_ids[sib0ID] = 2 * heapID + 1;
		mesh_data.bisector_ids[sib1ID] = 4 * heapID + 1;
		out_neighbors[selfID] = {sib1ID, res0X, sib0ID};
		out_neighbors[sib0ID] = {res1Y, selfID, n.next};
		out_neighbors[sib1ID] = {res0Y, selfID, res1X};

		auto b = bisector;
		b.propagationID = selfID;
		b.modified = true;

		b.problem_neighbor = ~0u;
		mesh_data.bisector_data[selfID] = b;

		b.problem_neighbor = n.next;
		mesh_data.bisector_data[sib0ID] = b;

		b.problem_neighbor = ~0u;
		mesh_data.bisector_data[sib1ID] = b;

		queue_id (&mesh_data.queues.propagate[0], sib0ID);
	} else if (subdiv == (center_split|left_split)) {
		uint res0X, res0Y;
		uint res1X = ~0u, res1Y = ~0u;
		evaluate_neighbors (selfID, n.next, res0X, res0Y);
		if (n.twin != ~0u) {
			evaluate_neighbors (selfID, n.twin, res1X, res1Y);
		}

		mesh_data.bisector_ids[selfID] = 2 * heapID;
		mesh_data.bisector_ids[sib0ID] = 4 * heapID + 2;
		mesh_data.bisector_ids[sib1ID] = 4 * heapID + 3;
		out_neighbors[selfID] = {sib1ID, res1X, n.prev};
		out_neighbors[sib0ID] = {sib1ID, res0X, res1Y};
		out_neighbors[sib1ID] = {res0Y, sib0ID, selfID};

		auto b = bisector;
		b.propagationID = selfID;
		b.modified = true;
		b.problem_neighbor = ~0u;

		mesh_data.bisector_data[selfID] = b;
		mesh_data.bisector_data[sib0ID] = b;
		mesh_data.bisector_data[sib1ID] = b;
	} else if (subdiv == (center_split|right_split|left_split)) {
		uint res0X, res0Y;
		uint res1X, res1Y;
		uint res2X = ~0u, res2Y = ~0u;
		evaluate_neighbors (selfID, n.prev, res0X, res0Y);
		evaluate_neighbors (selfID, n.next, res1X, res1Y);
		if (n.twin != ~0u) {
			evaluate_neighbors (selfID, n.twin, res2X, res2Y);
		}

		mesh_data.bisector_ids[selfID] = 4 * heapID;
		mesh_data.bisector_ids[sib0ID] = 4 * heapID + 2;
		mesh_data.bisector_ids[sib1ID] = 4 * heapID + 1;
		mesh_data.bisector_ids[sib2ID] = 4 * heapID + 3;
		out_neighbors[selfID] = {sib1ID, res0X, sib2ID};
		out_neighbors[sib0ID] = {sib2ID, res1X, res2Y};
		out_neighbors[sib1ID] = {res0Y, selfID, res2X};
		out_neighbors[sib2ID] = {res1Y, sib0ID, selfID};

		auto b = bisector;
		b.propagationID = selfID;
		b.modified = true;
		b.problem_neighbor = ~0u;

		mesh_data.bisector_data[selfID] = b;
		mesh_data.bisector_data[sib0ID] = b;
		mesh_data.bisector_data[sib1ID] = b;
		mesh_data.bisector_data[sib2ID] = b;
	}
	uint count = countbits (subdiv);
	for (uint i = 0; i < count; i++) {
		cbt.set_bit_buffer (bisector.indices[i], 1);
	}
}
}

@namespace propagate.bisect {
[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	if (threadID > mesh_data.queues.propagate[0].count) {
		return;
	}
	uint selfID = mesh_data.queues.propagate[0].entries[threadID];
	auto bisector = mesh_data.bisector_data[selfID];
	uint parentID = bisector.propagationID;
	uint target = bisector.problem_neighbor;
	uint subdiv = mesh_data.bisector_data[target].subdiv;
	uint sib1 = mesh_data.bisector_data[target].indices[1];
	auto neighbors = mesh_data.neighbors[neighbor_inds.next];
	auto t_n = neighbors[target];
	if (subdiv == 0) {
		if (t_n.prev == parentID) neighbors[target].prev = selfID;
		if (t_n.next == parentID) neighbors[target].next = selfID;
		if (t_n.twin == parentID) neighbors[target].twin = selfID;
	}
	if (subdiv == center_split) {
		uint prop = mesh_data.bisector_data[target].propagationID;
		auto p_n = neighbors[prop];
		if (t_n.twin == parentID) neighbors[target].twin = selfID;
		if (p_n.twin == parentID) neighbors[prop].twin = selfID;
	}
	if (subdiv == (center_split|right_split)) {
		neighbors[sib1].twin = selfID;
	}
	if (subdiv == (center_split|left_split)) {
		neighbors[target].twin = selfID;
	}
	bisector.problem_neighbor = ~0u;
	bisector.state = unchanged_element;
	mesh_data.bisector_data[selfID] = bisector;
}
}

@namespace simplify {
[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	if (threadID > mesh_data.queues.simplify.count) {
		return;
	}
	uint selfID = mesh_data.queues.simplify.entries[threadID];
	ulong heapID = mesh_data.bisector_ids[selfID];	// ALWAYS EVEN (only even queued)
	auto neighbors = mesh_data.neighbors[neighbor_inds.next];
	uint depth = heap_depth (heapID);
	uint pairID = neighbors[selfID].prev;
	// If the pair isn't at the same depth, then it was split, so can't merge
	// if the pair wasn't set to be simplified then it needs to be kept, so
	// still can't merge
	if (heap_depth (mesh_data.bisector_ids[pairID]) != depth
		|| mesh_data.bisector_data[pairID].state != simplify_element) {
		return;
	}
	uint twin_lo = neighbors[pairID].prev;
	uint twin_hi = neighbors[selfID].next;
	if (twin_lo != ~0u) {
		ulong loID = mesh_data.bisector_ids[twin_lo];
		if (heapID > loID) {
			return;
		}
		ulong hiID = mesh_data.bisector_ids[twin_hi];
		if (heap_depth (loID) != depth || heap_depth (hiID) != depth) {
			return;
		}
		if (mesh_data.bisector_data[twin_lo].state != simplify_element
			|| mesh_data.bisector_data[twin_hi].state != simplify_element) {
			return;
		}
	}
	queue_id (&mesh_data.queues.simplification, selfID);
}
}

@namespace simplification {
[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	if (threadID > mesh_data.queues.simplification.count) {
		return;
	}
	uint selfID = mesh_data.queues.simplification.entries[threadID];

	auto neighbors = mesh_data.neighbors[neighbor_inds.next];
	auto self = mesh_data.bisector_data[selfID];
	auto sn = neighbors[selfID];

	uint pairID = sn.prev;
	auto pair = mesh_data.bisector_data[pairID];
	auto pn = neighbors[pairID];

	uint twin_loID = pn.prev;
	uint twin_hiID = sn.next;

	mesh_data.bisector_ids[selfID] /= 2;
	mesh_data.bisector_ids[pairID] = 0;

	neighbors[selfID] = { sn.twin, pn.twin, twin_loID };

	self.propagationID = pairID;
	self.problem_neighbor = pn.twin;
	self.state = merged_element;
	self.modified = true;
	mesh_data.bisector_data[selfID] = self;

	if (self.problem_neighbor != ~0u) {
		queue_id (&mesh_data.queues.propagate[1], selfID);
	}

	pair.state = merged_element;
	pair.visible = 0;
	pair.modified = 0;
	mesh_data.bisector_data[pairID] = pair;

	cbt.set_bit_buffer (pairID, false);

	if (twin_loID != ~0u) {
		mesh_data.bisector_ids[twin_loID] /= 2;
		mesh_data.bisector_ids[twin_hiID] = 0;

		auto twin_lo = mesh_data.bisector_data[twin_loID];
		auto ln = neighbors[twin_loID];
		auto twin_hi = mesh_data.bisector_data[twin_hiID];
		auto hn = neighbors[twin_hiID];

		neighbors[twin_loID] = { ln.twin, hn.twin, selfID };

		twin_lo.propagationID = twin_hiID;
		twin_lo.problem_neighbor = hn.twin;
		twin_lo.state = merged_element;
		twin_lo.modified = true;
		mesh_data.bisector_data[twin_loID] = twin_lo;

		if (twin_lo.problem_neighbor != ~0u) {
			queue_id (&mesh_data.queues.propagate[1], twin_loID);
		}

		twin_hi.state = merged_element;
		twin_hi.visible = 0;
		twin_hi.modified = 0;
		mesh_data.bisector_data[twin_hiID] = twin_hi;

		cbt.set_bit_buffer (twin_hiID, false);
	}
}
}

@namespace propagate.simplify {
void
update_neighbor (const uint neighborID, const uint deleted, const uint selfID)
{
	auto neighbors = mesh_data.neighbors[neighbor_inds.next];
	auto nn = neighbors[neighborID];
	if (nn.prev == deleted) neighbors[neighborID].prev = selfID;
	if (nn.next == deleted) neighbors[neighborID].next = selfID;
	if (nn.twin == deleted) neighbors[neighborID].twin = selfID;
}

[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	if (threadID > mesh_data.queues.propagate[1].count) {
		return;
	}
	uint selfID = mesh_data.queues.propagate[1].entries[threadID];
	auto self = mesh_data.bisector_data[selfID];
	uint deleted = self.propagationID;
	uint neighborID = self.problem_neighbor;

	auto neighbor = mesh_data.bisector_data[neighborID];

	if (neighbor.state != merged_element) {
		update_neighbor (neighborID, deleted, selfID);
	} else {
		if (!mesh_data.bisector_ids[neighborID]) {
			auto neighbors = mesh_data.neighbors[neighbor_inds.next];
			neighborID = neighbors[neighborID].next;
		}
		update_neighbor (neighborID, deleted, selfID);
	}
	self.problem_neighbor = ~0u;
	mesh_data.bisector_data[selfID] = self;
}
}

@namespace indexation {
[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
[capability(Int64)]
void
main ()
{
	uint selfID = cbt.decode_bit (threadID);
	if (selfID == ~0u) {
		return;
	}
	mesh_data.bisector_map[threadID] = selfID;
	auto draw = &mesh_data.draw;

	uint slot;
	slot = atomicAdd (draw.indirect[0].vertexCount, 3);
	draw.bisector_indices[slot / 3] = selfID;

	auto self = mesh_data.bisector_data[selfID];

	if (!self.visible) {
		return;
	}

	slot = atomicAdd (draw.indirect[1].vertexCount, 3);
	draw.visible_indices[slot / 3] = selfID;

	if (!self.modified) {
		return;
	}
	slot = atomicAdd (draw.modified_bisector_count, 4);
	draw.modified_indices[slot / 4] = selfID;
}

#define RUP(x, m) (((x) + ((m) - 1)) & ~((m) - 1))
#define WGS(x) (RUP (x, WORKGROUP_SIZE) / WORKGROUP_SIZE)

[shader(GLCompute, LocalSize=[1,1,1])]
[capability(Int64)]
void
prepare ()
{
	auto draw = &mesh_data.draw;
	uint vertexCount = draw.indirect[0].vertexCount;
	draw.dispatch[0] = {WGS (vertexCount / 3), 1, 1};
	draw.dispatch[1] = {WGS (vertexCount * 4 / 3), 1, 1};
	draw.dispatch[2] = {WGS (draw.modified_bisector_count), 1, 1};
	draw.bisector_count = vertexCount / 3;
}
}

#if 0
#include <QF/math/bitop.h>
@namespace test {
@overload void
dispatch (void (*shader)(), uvec3 counts)
{
	//for (uint z = 0; z < counts.z; z++) {
	//	for (uint y = 0; y < counts.y; y++) {
			for (uint x = 0; x < counts.x; x++) {
				//GlobalInvocationId = {x, y, z};
				GlobalInvocationId = {x, 0, 0};
				shader ();
			}
	//	}
	//}
}

@overload void
dispatch (void (*shader)(), DispatchIndirect *indirect)
{
	dispatch (shader, *(uvec3 *) indirect);
}

void
initialize_bisectors ()
{
	draw.bisector_count = countof (halfedges);
	base_depth = BITOP_LOG2 (draw.bisector_count);
	ulong base_id = 1ul << base_depth;
	for (uint i = 0; i < countof (halfedges); i++) {
		auto b = &bisector_data[i];
		bisector_ids[i] = base_id + i;
		bisector_neighbors[i] = {
			.next = halfedges[i].next,
			.prev = halfedges[i].prev,
			.twin = halfedges[i].twin,
		};
		cbt.set_bit_buffer (i, 1);
	}
	const uint D = mesh_data.cbt.depth;
	for (uint i = countof(halfedges); i < 1u << D; i++) {
		cbt.set_bit_buffer (i, 0);
	}
	dispatch (cbt.first_pass, uvec3(bits_size, 1, 1));
	for (uint i = cbt_levels; i-- > 0; ) {
		cbt.reduce_level = i;
		dispatch (cbt.second_pass, uvec3(1 << i, 1, 1));
	}
	draw.indirect[0].vertexCount = countof (halfedges) * 3;
	dispatch (draw.update.main, uvec3(draw.bisector_count, 1, 1));
}

void
dump ()
{
	uint count = cbt.count ();
	auto neighbors = db_bisector_neighbors[neighbors_index];
	for (uint i = 0; i < count; i++) {
		uint selfID = cbt.decode_bit (i);
		ulong heapID = bisector_ids[selfID];
		auto n = neighbors[selfID];
		uint depth = heap_depth(heapID) - base_depth;
		uint vis = bisector_data[selfID].visible;
		heapID -= 1 << (depth + base_depth);
		printf ("i:%d:%-3d s:%-3d h:%d,%-4ld p:%-3d n:%-3d t:%-3d %v %v %v\n",
				vis, i, selfID, depth, heapID, n.prev, n.next, n.twin,
				draw.vertex_buffer[3 * selfID + 0],
				draw.vertex_buffer[3 * selfID + 1],
				draw.vertex_buffer[3 * selfID + 2]);
	}
}

}

void
update ()
{
	uint next = (neighbors_index + 1) & 1;
	auto curr_neighbors = db_bisector_neighbors[neighbors_index];
	auto next_neighbors = db_bisector_neighbors[next];
	// RESET
	// bind mesh cbt            CBT_BUFFER0_BINDING_SLOT + x
	// bind memory              MEMORY_BUFFER_BINDING_SLOT
	// bind mesh classify       CLASSIFICATION_BUFFER_BINDING_SLOT
	// bind mesh allocate       ALLOCATE_BUFFER_BINDING_SLOT
	// bind mesh indirect draw  INDIRECT_DRAW_BUFFER_BINDING_SLOT
	// bind mesh simplification SIMPLIFICATION_BUFFER_BINDING_SLOT
	// bind mesh propagate      PROPAGATE_BUFFER_BINDING_SLOT
	test.dispatch (reset.main, uvec3(1,1,1));
	// barrier memory

	// CLASSIFY
	// bind global constants      GLOBAL_CB_BINDING_SLOT
	// bind geometry constants    GEOMETRY_CB_BINDING_SLOT
	// bind update constants      UPDATE_CB_BINDING_SLOT
	// ====
	// bind mesh vertices         CURRENT_VERTEX_BUFFER_SLOT
	// bind mesh indexed bisector INDEXED_BISECTOR_BUFFER_BINDING_SLOT
	// ====
	// bind mesh indirect draw    INDIRECT_DRAW_BUFFER_BINDING_SLOT
	// bind mesh head id          HEAP_ID_BUFFER_BINDING_SLOT
	// bind mesh update           BISECTOR_DATA_BUFFER_BINDING_SLOT
	// bind mesh classification   CLASSIFICATION_BUFFER_BINDING_SLOT
	test.dispatch (classify.main, &draw.dispatch[0]);
	// barrier mesh update

	// PREPARE INDIRECT SPLIT
	// bind mesh classification   ALLOCATE_BUFFER_BINDING_SLOT
	// bind indirect              INDIRECT_DISPATCH_BUFFER_BINDING_SLOT
	queue_t *classify_queues[] = {&split.queue, &simplify.queue};
	dispatch.queues = classify_queues;
	test.dispatch (dispatch.prepare, uvec3(2, 1, 1));
	// barrier indirect

	// SPLIT
	// bind global constants      GLOBAL_CB_BINDING_SLOT
	// bind geometry constants    GEOMETRY_CB_BINDING_SLOT
	// bind update constants      UPDATE_CB_BINDING_SLOT
	// ====
	// bind mesh classification   CLASSIFICATION_BUFFER_BINDING_SLOT
	// bind mesh heap id          HEAP_ID_BUFFER_BINDING_SLOT
	// bind mesh update           BISECTOR_DATA_BUFFER_BINDING_SLOT
	// bind mesh cur neighbor     NEIGHBORS_BUFFER_BINDING_SLOT
	// bind memory                MEMORY_BUFFER_BINDING_SLOT
	// bind mesh allocate         ALLOCATE_BUFFER_BINDING_SLOT
	bisector_neighbors = curr_neighbors;
	bisector_output_neighbors = (neighbors_t*)-1;
	test.dispatch (split.main, &dispatch.indirect[0]);
	// barrier mesh update

	// PREPARE INDIRECT ALLOCATE
	// bind mesh allocate         ALLOCATE_BUFFER_BINDING_SLOT
	// bind indirect              INDIRECT_DISPATCH_BUFFER_BINDING_SLOT
	queue_t *allocate_queues[] = {&allocate.queue};
	dispatch.queues = allocate_queues;
	test.dispatch (dispatch.prepare, uvec3(1, 1, 1));
	// barrier indirect

	// ALLOCATE
	// bind global constants      GLOBAL_CB_BINDING_SLOT
	// bind geometry constants    GEOMETRY_CB_BINDING_SLOT
	// bind update constants      UPDATE_CB_BINDING_SLOT
	// ====
	// bind mesh cbt buffers      CBT_BUFFER0_BINDING_SLOT + i
	// bind mesh allocate         ALLOCATE_BUFFER_BINDING_SLOT
	// bind mesh update           BISECTOR_DATA_BUFFER_BINDING_SLOT
	// bind memory                MEMORY_BUFFER_BINDING_SLOT
	test.dispatch (allocate.main, &dispatch.indirect[0]);
	// barrier update

	// copy mesh cur neighbor to mesh next neighbor
	for (uint i = 0; i < countof (db_bisector_neighbors[0]); i++) {
		next_neighbors[i] = curr_neighbors[i];
	}

	// BISECT
	// bind global constants      GLOBAL_CB_BINDING_SLOT
	// bind geometry constants    GEOMETRY_CB_BINDING_SLOT
	// bind update constants      UPDATE_CB_BINDING_SLOT
	// ====
	// bind mesh cbt              CBT_BUFFER0_BINDING_SLOT + i
	// bind mesh allocate         ALLOCATE_BUFFER_BINDING_SLOT
	// bind mesh heap id          HEAP_ID_BUFFER_BINDING_SLOT
	// bind mesh cur neighbor     NEIGHBORS_BUFFER_BINDING_SLOT
	// bind mesh next neighbor    NEIGHBORS_OUTPUT_BUFFER_BINDING_SLOT
	// bind mesh propagate        PROPAGATE_BUFFER_BINDING_SLOT
	bisector_neighbors = curr_neighbors;
	bisector_output_neighbors = next_neighbors;
	test.dispatch (bisect.main, &dispatch.indirect[0]);
	// barrier mesh next neighbor

	// PREPARE INDIRECT PROPAGATE BISECT
	// bind mesh propagate        ALLOCATE_BUFFER_BINDING_SLOT
	// bind indirect              INDIRECT_DISPATCH_BUFFER_BINDING_SLOT
	queue_t *propagate_bisect_queues[] = {&propagate.queue[0]};
	dispatch.queues = propagate_bisect_queues;
	test.dispatch (dispatch.prepare, uvec3(1, 1, 1));
	// barrier indirect

	// PROPAGATE SPLIT/BISECT
	// bind global constants      GLOBAL_CB_BINDING_SLOT
	// bind geometry constants    GEOMETRY_CB_BINDING_SLOT
	// bind update constants      UPDATE_CB_BINDING_SLOT
	// ====
	// bind mesh propagate        PROPAGATE_BUFFER_BINDING_SLOT
	// bind mesh update           BISECTOR_DATA_BUFFER_BINDING_SLOT
	// bind mesh next neighbor    NEIGHBORS_BUFFER_BINDING_SLOT
	bisector_neighbors = next_neighbors;
	bisector_output_neighbors = (neighbors_t*)-1;
	test.dispatch (propagate.bisect.main, &dispatch.indirect[0]);
	// barrier mesh update

	// PREPARE SIMPLIFY
	// bind global constants      GLOBAL_CB_BINDING_SLOT
	// bind geometry constants    GEOMETRY_CB_BINDING_SLOT
	// bind update constants      UPDATE_CB_BINDING_SLOT
	// ====
	// bind mesh classification   CLASSIFICATION_BUFFER_BINDING_SLOT
	// bind mesh heap id          HEAP_ID_BUFFER_BINDING_SLOT
	// bind mesh update           BISECTOR_DATA_BUFFER_BINDING_SLOT
	// bind mesh next neighbor    NEIGHBORS_BUFFER_BINDING_SLOT
	// bind mesh simplification   SIMPLIFICATION_BUFFER_BINDING_SLOT
	bisector_neighbors = next_neighbors;
	bisector_output_neighbors = (neighbors_t*)-1;
	test.dispatch (simplify.main, &dispatch.indirect[1]);
	// barrier mesh simplification

	// PREPARE INDIRECT SIMPLIFY
	// bind mesh simplification   ALLOCATE_BUFFER_BINDING_SLOT
	// bind indirect              INDIRECT_DISPATCH_BUFFER_BINDING_SLOT
	queue_t *simplification_queues[] = {&simplification.queue};
	dispatch.queues = simplification_queues;
	test.dispatch (dispatch.prepare, uvec3(1, 1, 1));
	// barrier indirect

	// SIMPLIFY
	// bind global constants      GLOBAL_CB_BINDING_SLOT
	// bind geometry constants    GEOMETRY_CB_BINDING_SLOT
	// bind update constants      UPDATE_CB_BINDING_SLOT
	// ====
	// bind mesh simplification   SIMPLIFICATION_BUFFER_BINDING_SLOT
	// bind mesh update           BISECTOR_DATA_BUFFER_BINDING_SLOT
	// bind mesh next neighbor    NEIGHBORS_BUFFER_BINDING_SLOT
	// bind mesh heap id          HEAP_ID_BUFFER_BINDING_SLOT
	// bind mesh cbt              CBT_BUFFER0_BINDING_SLOT + i
	// bind mesh propagate        PROPAGATE_BUFFER_BINDING_SLOT
	bisector_neighbors = next_neighbors;
	bisector_output_neighbors = (neighbors_t*)-1;
	test.dispatch (simplification.main, &dispatch.indirect[0]);
	// barrier mesh next neighbor

	// PREPARE INDIRECT PROPAGATE SIMPLIFY
	// bind mesh propagate        ALLOCATE_BUFFER_BINDING_SLOT
	// bind indirect              INDIRECT_DISPATCH_BUFFER_BINDING_SLOT
	queue_t *propagate_simplify_queues[] = {&propagate.queue[0], &propagate.queue[1]};
	dispatch.queues = propagate_simplify_queues;
	test.dispatch (dispatch.prepare, uvec3(2, 1, 1));
	// barrier indirect

	// PROPAGATE SIMPLIFY
	// bind global constants      GLOBAL_CB_BINDING_SLOT
	// bind geometry constants    GEOMETRY_CB_BINDING_SLOT
	// bind update constants      UPDATE_CB_BINDING_SLOT
	// ====
	// bind mesh propagate        PROPAGATE_BUFFER_BINDING_SLOT
	// bind mesh heap id          HEAP_ID_BUFFER_BINDING_SLOT
	// bind mesh update           BISECTOR_DATA_BUFFER_BINDING_SLOT
	// bind mesh next neighbor    NEIGHBORS_BUFFER_BINDING_SLOT
	bisector_neighbors = next_neighbors;
	bisector_output_neighbors = (neighbors_t*)-1;
	test.dispatch (propagate.simplify.main, &dispatch.indirect[1]);
	// barrier mesh next neighbor

	// UPDATE TREE
	// bind mesh cbt double buffer CBT_BUFFER0_BINDING_SLOT + b
	// dispatch reduce prepass {last_level_size / (4*WGS), 1, 1}
	// barrier mesh cbt 0
	// dispatch reduce first pass {8, 1, 1}
	// barrier mesh cbt 0
	// dispatch reduce second pass {1, 1, 1}
	// barrier mesh cbt 0
	test.dispatch (cbt.first_pass, uvec3(bits_size, 1, 1));
	for (uint i = cbt_levels; i-- > 0; ) {
		cbt.reduce_level = i;
		test.dispatch (cbt.second_pass, uvec3(1 << i, 1, 1));
	}

	// mesh cur neighbor = mesh next neighbor (rotate)
	neighbors_index = next;

	// PREPARE INDIRECT DRAW & DISPATCH
	// bind geometry constants    GEOMETRY_CB_BINDING_SLOT
	// ====
	// bind mesh heap id          HEAP_ID_BUFFER_BINDING_SLOT
	// bind mesh indirect draw    INDIRECT_DRAW_BUFFER_BINDING_SLOT
	// bind mesh indexed          BISECTOR_INDICES_BINDING_SLOT
	// bind mesh update           BISECTOR_DATA_BUFFER_BINDING_SLOT
	// bind mesh cur neighbor     NEIGHBORS_BUFFER_BINDING_SLOT
	// bind mesh visible indexed  VISIBLE_BISECTOR_INDICES_BINDING_SLOT
	// bind mesh modified indexed MODIFIED_BISECTOR_INDICES_BINDING_SLOT
	// numGroups = (mesh.totalNumElements + WGS - 1) / WGS;
	uint numGroups = WGS (max_bisector_count);
	bisector_neighbors = db_bisector_neighbors[neighbors_index];
	bisector_output_neighbors = (neighbors_t*)-1;
	test.dispatch (indexation.main, uvec3 (numGroups, 1, 1));
	// barrier mesh indirect draw

	// PREPARE BISECTOR INDIRECT
	// bind mesh indirect draw     INDIRECT_DRAW_BUFFER_BINDING_SLOT
	// bind mesh indirect dispatch INDIRECT_DISPATCH_BUFFER_BINDING_SLOT
	test.dispatch (indexation.prepare, uvec3(1, 1, 1));

	test.dispatch (draw.update.main, uvec3(draw.bisector_count, 1, 1));
}

int
main()
{
	static mat4 view = {
		{     1,    0,    0, 0 },
		{     0,   -1,    0, 0 },
		{     0,    0,   -1, 0 },
		{-4.125, 4.25, 0.25, 1 },
	};
	static mat4 proj = {
		{ 1, 0,      0, 0 },
		{ 0, 1,      0, 0 },
		{ 0, 0,      0, 1 },
		{ 0, 0, 0.0625, 0 },
	};
	draw.ProjView = proj * view;
	draw.camera_fwd = {0, 0, -1};
	draw.camera_pos = {4.125, 4.25, 0.25};
	draw.frustum_planes[0] = {-1, 0, -1, 4.375 };
	draw.frustum_planes[1] = { 1, 0, -1,-3.875 };
	draw.frustum_planes[2] = { 0,-1, -1, 4.5 };
	draw.frustum_planes[3] = { 0, 1, -1,-4.0 };
	triangle_size = 0.25*0.25;
	bisector_neighbors = db_bisector_neighbors[neighbors_index];
	test.initialize_bisectors ();
	printf ("base_depth: %ld\n", base_depth);
	cbt.dump ();
	test.dump ();

	max_depth = base_depth + 8;
	update();
	uint old_mem;
	do {
		old_mem = cbt.count();
		update();
	} while (old_mem != cbt.count());
	cbt.dump ();
	test.dump ();

	max_depth = base_depth + 0;
	do {
		old_mem = cbt.count();
		printf ("%d\n", old_mem);
		update();
	} while (old_mem != cbt.count());
	cbt.dump ();
	test.dump ();

	return 0;
}
#endif
