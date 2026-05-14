#include <QF/math/bitop.h>
#include <QF/qfmodel.h>

void printf(string fmt, ...)=#0;
void traceon () = #0;
void traceoff () = #0;

typedef struct DrawIndirect {
	uint        vertexCount;
	uint        instanceCount;
	uint        firstVertex;
	uint        firstInstance;
} DrawIndirect;

typedef struct DispatchIndirect {
	uint        x;
	uint        y;
	uint        z;
} DispatchIndirect;

typedef struct MemoryBuffer {
	uint        allocated;
	uint        available;
} MemoryBuffer;

typedef struct queue_s {
	uint        count;
	uint       *entries;
} queue_t;

typedef struct bisector_s {
	uint        propagationID;
	uint        problem_neighbor;
	int         state:3;
	uint        visible:1;
	uint        modified:1;
	uint        subdiv;

	uint        indices[3];
} bisector_t;

typedef struct neighbors_s {
	uint        prev;
	uint        next;
	uint        twin;
} neighbors_t;

#define D 7
#define max_bisector_count (1 << D)
#define WORKGROUP_SIZE 64

@namespace draw {
DrawIndirect indirect[2];
uint bisector_indices[max_bisector_count];
uint visible_indices[max_bisector_count];
uint modified_indices[max_bisector_count];
vec3 vertex_buffer[4 * max_bisector_count];
uint modified_bisector_count;
uint bisector_count;

mat4 ProjView;
vec3 camera_fwd;
vec3 camera_pos;
vec4 frustum_planes[4];
};

float triangle_size;

DispatchIndirect dispatch[3];
MemoryBuffer memory;
@namespace split {
queue_t queue;
}
@namespace simplify {
queue_t queue;
}
@namespace allocate {
queue_t queue;
}
@namespace propagate {
queue_t queue[2];
}
@namespace simplification {
queue_t queue;
}

//[in("GlobalInvocationId")]
uvec3 GlobalInvocationId;
#define threadID GlobalInvocationId.x

uint base_depth;
uint max_depth;

halfedge_t halfedges[] = {
	{  6,  1,  3,  3,  3,  0 },
	{  7,  2,  0,  2,  4,  0 },
	{ -1,  3,  1,  1,  0,  0 },
	{ -1,  0,  2,  0,  1,  0 },
	{ -1,  5,  6,  3,  2,  1 },
	{  8,  6,  4,  4,  5,  1 },
	{  0,  4,  5,  2,  3,  1 },
	{  1,  8, 11,  1,  4,  2 },
	{  5,  9,  7,  2,  5,  2 },
	{ -1, 10,  8,  4,  6,  2 },
	{ -1, 11,  9,  5,  7,  2 },
	{ -1,  7, 10,  6,  8,  2 },
};

vec3 vertices[] = {
	{ 1, 4, 0 },
	{ 4, 6, 0 },
	{ 3, 3, 0 },
	{ 0, 1, 0 },
	{ 5, 0, 0 },
	{ 8, 2, 0 },
	{ 7, 5, 0 },
};

uint bisector_map[max_bisector_count];
bisector_t bisector_data[max_bisector_count];
neighbors_t bisector_neighbors[max_bisector_count];
ulong bisector_ids[max_bisector_count];

uint atomicOr(@reference(uint) mem, const uint data)
{
	uint old = mem;
	mem |= data;
	return old;
}

uint atomicAnd(@reference(uint) mem, const uint data)
{
	uint old = mem;
	mem &= data;
	return old;
}

@namespace cbt {
#define group_levels 5
#define bits_size (1u << (D - group_levels))
#define group_base bits_size
#define cbt_size (group_base + group_levels * bits_size + bits_size)
#define cbt_levels (D - group_levels)
#define bits_base (group_base + group_levels * bits_size)
uint heap[cbt_size] = { 1 << D };
uint size ()
{
	return heap[0];
}

uint count ()
{
	return heap[1];
}

uint decode_bit (uint index)
{
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
	if (index >= heap[0] - heap[1]) {
		return -1;
	}
	uint bitID = 1;
	uint c = 1 << (D - 1);
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

void set_bit_buffer (uint bitID, uint val)
{
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
	uint ind = threadID + (1 << reduce_level);
	heap[ind] = heap[ind * 2] + heap[ind * 2 + 1];
}

void dump()
{
	for (uint i = 0; i < (1u << (D - group_levels + 1)); i++) {
		printf ("%s%d", (i & (i - 1)) ? " " : "\n", heap[i]);
	}
	printf ("\n");
	for (uint i = 0; i < group_levels; i++) {
		for (uint j = 0; j < bits_size; j++) {
			printf (" %08x", heap[(2 + i) * bits_size + j]);
		}
		printf ("\n");
	}
}

}

uint findMSB(ulong x)
{
	uint count = 0;
	while (x > 1) {
		x >>= 1;
		count++;
	}
	return count;
}

uint countbits (uint x)
{
	uint c = 0;
	for (; x; c++) {
		x &= x - 1;
	}
	return c;
}

uint
heap_depth (ulong x)
{
	return findMSB (x);
}

@overload uint atomicAdd(@reference(uint) mem, const uint data)
{
	uint old = mem;
	mem += data;
	return old;
}

@overload uint atomicAdd(@reference(int) mem, const int data)
{
	uint old = mem;
	mem += data;
	return old;
}

void
queue_id (@reference(queue_t) queue, uint id)
{
	uint slot = atomicAdd (queue.count, 1);
	queue.entries[slot] = id;
}

bool
reserve_memory (int amount)
{
	int old = atomicAdd (memory.available, -amount);
	if (old < amount) {
		atomicAdd (memory.available, amount);
		return false;
	}
	return true;
}

uint
allocate_memory (int amount)
{
	return atomicAdd (memory.allocated, amount);
}

bool
set_subdiv (uint bisector, uint subdiv)
{
	uint old = bisector_data[bisector].subdiv;
	bisector_data[bisector].subdiv |= subdiv;//FIXME make atomic
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

@generic (genvec = @vector(float)) {
genvec
min(genvec a, genvec b)
{
	@uint(genvec) m = a > b;
	return (genvec) ((@uint(genvec)) a & ~m)
		 + (genvec) ((@uint(genvec)) b &  m);
}

genvec
max(genvec a, genvec b)
{
	@uint(genvec) m = a < b;
	return (genvec) ((@uint(genvec)) a & ~m)
		 + (genvec) ((@uint(genvec)) b &  m);
}

genvec
sign(genvec x)
{
	@uint(genvec) m = x < @construct(genvec, 0);
	auto p1 = @construct(genvec,  1);
	auto m1 = @construct(genvec, -1);
	return (genvec) ((@uint(genvec)) p1 & ~m)
		 + (genvec) ((@uint(genvec)) m1 &  m);
}
};

@overload float sqrt(float x) = #0;
vector normalize (vector x)
{
	return x / sqrt (x • x);
}

@namespace draw.update {
//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
mat3
RootBisectorVertices(uint halfedgeID)
{
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
BisectorVertices (ulong heapID)
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
	ulong base = 1 << base_depth;
	while (h >= base << 1) {
		uint b = (uint) h & 1;
		M = Mb[b] * M;
		h >>= 1;
	}
	h &= base - 1;
	return RootBisectorVertices (h) * M;
}

void
main ()
{
	if (threadID >= indirect[0].vertexCount / 3) {
		return;
	}
	//traceon ();
	uint selfID = cbt.decode_bit (threadID);
	ulong heapID = bisector_ids[selfID];
	printf ("%d %d %ld %d\n", threadID, selfID, heapID,
			indirect[0].vertexCount);
	auto v = BisectorVertices (heapID);
	vertex_buffer[3 * selfID + 0] = v[0];
	vertex_buffer[3 * selfID + 1] = v[1];
	vertex_buffer[3 * selfID + 2] = v[2];
	//traceoff ();
}
}

@namespace reset {
//[shader(GLCompute, LocalSize=[1,1,1])]
void
main ()
{
	memory = {
		.allocated = 0,
		.available = cbt.size() - cbt.count(),
	};
	split.queue.count = 0;
	simplify.queue.count = 0;
	allocate.queue.count = 0;
	propagate.queue[0].count = 0;
	propagate.queue[1].count = 0;
	simplification.queue.count = 0;
	draw.indirect = {
		{ 0, 1, 0, 0 },
		{ 0, 1, 0, 0 },
	};
	draw.modified_bisector_count = 0;
}
}

@namespace classify {

bool
frustum_aabb_intersect (vec4 aabb_min, vec4 aabb_max)
{
	auto center = (aabb_max + aabb_min) * 0.5;
	auto extents = (aabb_max - aabb_min) * 0.5;
	for (int i = 0; i < countof (draw.frustum_planes); i++) {
		auto plane = draw.frustum_planes[i];
		auto tp = center + sign (plane) * extents;
		if (tp • plane < 0) {
			return false;
		}
	}
	return true;
}


int
classify_element (uint selfID, uint depth)
{
	vec3 points[4] = {
		draw.vertex_buffer[3 * selfID + 0],
		draw.vertex_buffer[3 * selfID + 1],
		draw.vertex_buffer[3 * selfID + 2],
		draw.vertex_buffer[3 * max_bisector_count + selfID],
	};

	vec3 n = normalize((points[1] - points[0]) × (points[2] - points[0]));
	vec3 c = (points[0] + points[1] + points[2]) / 3;
	vec3 vd = normalize (draw.camera_pos - c);
	float fv = vd • draw.camera_fwd;
	float vn = vd • n;

	if (fv < 0 || vn < -1e-3) {
		return back_face_culled;
	}

	vec4 aabbMin = [min (points[0], min (points[1], points[2])), 1];
	vec4 aabbMax = [max (points[0], max (points[1], points[2])), 1];
	if (!frustum_aabb_intersect (aabbMin, aabbMax)) {
		return frustum_culled;
	}

	vec4 p0 = draw.ProjView * [points[0], 1];
	vec4 p1 = draw.ProjView * [points[1], 1];
	vec4 p2 = draw.ProjView * [points[2], 1];
	p0 /= p0.w;
	p1 /= p1.w;
	p2 /= p2.w;
	vec2 pa = p1.xy - p0.xy;
	vec2 pb = p2.xy - p0.xy;
	float area = (pa.x * pb.y - pa.y * pb.x) * 0.5;

	if (area > triangle_size && depth < max_depth) {
		return bisect_element;
	}
	if (area < triangle_size * 0.5 || depth > max_depth) {
		vec4 p3 = draw.ProjView * [points[3], 1];
		p3 /= p3.w;
		vec2 pc = p3.xy - p0.xy;
		float parent_area = (pa.x * pc.y - pa.y * pc.x) * 0.5;
		if (area < triangle_size || depth > max_depth) {
			return too_small;
		}
	}
	return unchanged_element;
}

//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
void
main ()
{
	if (threadID > draw.bisector_count) {
		return;
	}
	uint selfID = bisector_map[threadID];
	uint depth = heap_depth (bisector_ids[selfID]);
	int valid = classify_element (selfID, depth);
	auto self = bisector_data[selfID];
	self.modified = 0;
	self.visible = 0;
	if (valid > unchanged_element) {
		self.state = bisect_element;
		queue_id (split.queue, selfID);
	}
	self.visible = valid >= too_small;
	if (valid < unchanged_element && depth != base_depth) {
		self.state = simplify_element;

		ulong heapID = bisector_ids[selfID];
		// Register only even heapIDs as odd ones will be taken care of by
		// the even ones
		if (!(heapID & 1)) {
			queue_id (simplify.queue, selfID);
		}
	}
	bisector_data[selfID] = self;
}

//[shader(GLCompute, LocalSize=[1,1,1])]
void
prepare ()
{
	dispatch[0].x = split.queue.count;
	dispatch[1].x = simplify.queue.count;
}
}

@namespace split {
//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
void
main ()
{
	if (threadID > queue.count) {
		return;
	}
	uint selfID = queue.entries[threadID];
	auto n = bisector_neighbors[selfID];
	if (n.prev != ~0u) {
		// on path
		if (bisector_neighbors[n.prev].twin == selfID
			&& bisector_data[n.prev].state != unchanged_element) {
			return;
		}
	}
	if (n.next != ~0u) {
		// on path
		if (bisector_neighbors[n.next].twin == selfID
			&& bisector_data[n.next].state != unchanged_element) {
			return;
		}
	}
	ulong heapID = bisector_ids[selfID];
	uint depth = heap_depth (heapID);
	int max_memory = 2 * (depth - base_depth) - 1;
	if (n.twin == ~0u) {
		// on the edge of the mesh
		max_memory = 1;
	} else if (bisector_neighbors[n.twin].twin == selfID) {
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

	queue_id (allocate.queue, selfID);

	uint twinID = n.twin;
	uint used_memory = 1;
	bool done = false;
	while (!done) {
		if (twinID == ~0u) {
			break;
		}
		uint t_depth = heap_depth (bisector_ids[twinID]);
		if (t_depth == depth) {
			// both triangles are at the same detph
			if (set_subdiv (twinID, center_split)) {
				queue_id (allocate.queue, twinID);
			}
			done = true;
		} else {
			auto tn = bisector_neighbors[twinID];
			// either twin.prev or twin.next == selfID
			uint subdiv = bisector_neighbors[twinID].prev == selfID
				? center_split | right_split
				: center_split | left_split;
			if (!set_subdiv (twinID, subdiv)) {
				used_memory++;
				done = true;
			} else {
				queue_id (allocate.queue, twinID);
				// need two splits
				used_memory += 2;
				selfID = twinID;
				depth = t_depth;
				twinID = bisector_neighbors[selfID].twin;
			}
		}
	}
	reserve_memory (used_memory - max_memory);
}

//[shader(GLCompute, LocalSize=[1,1,1])]
void
prepare ()
{
	dispatch[0].x = allocate.queue.count;
}
}

@namespace allocate {
//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
void
main ()
{
	if (threadID > queue.count) {
		return;
	}
	uint selfID = queue.entries[threadID];
	auto bisector = bisector_data[selfID];
	if (bisector.subdiv) {
		uint count = countbits (bisector.subdiv);
		uint first = allocate_memory (count);
		for (uint i = 0; i < count; i++) {
			uint index = cbt.decode_bit_complement (first + i);
			bisector.indices[i] = index;
		}
		bisector_data[selfID] = bisector;
	}
}
}

@namespace bisect {
void
evaluate_neighbors (uint selfID, uint twinID, @out uint resX, @out uint resY)
{
	auto twin = bisector_data[twinID];
	auto n = bisector_neighbors[twinID];
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

//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
void
main ()
{
	if (threadID > allocate.queue.count) {
		return;
	}
	uint selfID = allocate.queue.entries[threadID];
	ulong heapID = bisector_ids[selfID];
	auto bisector = bisector_data[selfID];
	auto n = bisector_neighbors[selfID];
	uint subdiv = bisector.subdiv;
	uint sib0ID = bisector.indices[0];
	uint sib1ID = bisector.indices[1];
	uint sib2ID = bisector.indices[2];
	if (subdiv == center_split) {
		uint resX = ~0u, resY = ~0u;
		if (n.twin != ~0u) {
			evaluate_neighbors (selfID, n.twin, resX, resY);
		}

		bisector_ids[selfID] = 2 * heapID;
		bisector_ids[sib0ID] = 2 * heapID + 1;
		bisector_neighbors[selfID] = {sib0ID, resX, n.prev};
		bisector_neighbors[sib0ID] = {resY, selfID, n.next};

		auto b = bisector;
		b.propagationID = selfID;
		b.modified = true;

		b.problem_neighbor = ~0u;
		bisector_data[selfID] = b;

		b.problem_neighbor = n.next;
		bisector_data[sib0ID] = b;

		queue_id (propagate.queue[0], sib0ID);
	} else if (subdiv == (center_split|right_split)) {
		uint res0X, res0Y;
		uint res1X = ~0u, res1Y = ~0u;
		evaluate_neighbors (selfID, n.prev, res0X, res0Y);
		if (n.twin != ~0u) {
			evaluate_neighbors (selfID, n.twin, res1X, res1Y);
		}

		bisector_ids[selfID] = 4 * heapID;
		bisector_ids[sib0ID] = 2 * heapID + 1;
		bisector_ids[sib1ID] = 4 * heapID + 1;
		bisector_neighbors[selfID] = {sib1ID, res0X, sib0ID};
		bisector_neighbors[sib0ID] = {res1Y, selfID, n.next};
		bisector_neighbors[sib1ID] = {res0Y, selfID, res1X};

		auto b = bisector;
		b.propagationID = selfID;
		b.modified = true;

		b.problem_neighbor = ~0u;
		bisector_data[selfID] = b;

		b.problem_neighbor = n.next;
		bisector_data[sib0ID] = b;

		b.problem_neighbor = ~0u;
		bisector_data[sib1ID] = b;

		queue_id (propagate.queue[0], sib0ID);
	} else if (subdiv == (center_split|left_split)) {
		uint res0X, res0Y;
		uint res1X = ~0u, res1Y = ~0u;
		evaluate_neighbors (selfID, n.next, res0X, res0Y);
		if (n.twin != ~0u) {
			evaluate_neighbors (selfID, n.twin, res1X, res1Y);
		}

		bisector_ids[selfID] = 2 * heapID;
		bisector_ids[sib0ID] = 4 * heapID + 2;
		bisector_ids[sib1ID] = 4 * heapID + 3;
		bisector_neighbors[selfID] = {sib1ID, res1X, n.prev};
		bisector_neighbors[sib0ID] = {sib1ID, res0X, res1Y};
		bisector_neighbors[sib1ID] = {res0Y, sib0ID, selfID};

		auto b = bisector;
		b.propagationID = selfID;
		b.modified = true;
		b.problem_neighbor = ~0u;

		bisector_data[selfID] = b;
		bisector_data[sib0ID] = b;
		bisector_data[sib1ID] = b;
	} else if (subdiv == (center_split|right_split|left_split)) {
		uint res0X, res0Y;
		uint res1X, res1Y;
		uint res2X = ~0u, res2Y = ~0u;
		evaluate_neighbors (selfID, n.prev, res0X, res0Y);
		evaluate_neighbors (selfID, n.next, res1X, res1Y);
		if (n.twin != ~0u) {
			evaluate_neighbors (selfID, n.twin, res2X, res2Y);
		}

		bisector_ids[selfID] = 4 * heapID;
		bisector_ids[sib0ID] = 4 * heapID + 2;
		bisector_ids[sib1ID] = 4 * heapID + 1;
		bisector_ids[sib2ID] = 4 * heapID + 3;
		bisector_neighbors[selfID] = {sib1ID, res0X, sib2ID};
		bisector_neighbors[sib0ID] = {sib2ID, res1X, res2Y};
		bisector_neighbors[sib1ID] = {res0Y, selfID, res2X};
		bisector_neighbors[sib2ID] = {res1Y, sib0ID, selfID};

		auto b = bisector;
		b.propagationID = selfID;
		b.modified = true;
		b.problem_neighbor = ~0u;

		bisector_data[selfID] = b;
		bisector_data[sib0ID] = b;
		bisector_data[sib1ID] = b;
		bisector_data[sib2ID] = b;
	}
	uint count = countbits (subdiv);
	for (uint i = 0; i < count; i++) {
		cbt.set_bit_buffer (bisector.indices[i], 1);
	}
}
}

@namespace propagate.bisect {
//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
void
main ()
{
	if (threadID > queue[0].count) {
		return;
	}
	uint selfID = queue[0].entries[threadID];
	auto bisector = bisector_data[selfID];
	uint parentID = bisector.propagationID;
	uint target = bisector.problem_neighbor;
	uint subdiv = bisector_data[target].subdiv;
	uint sib1 = bisector_data[target].indices[1];
	auto t_n = bisector_neighbors[target];
	if (subdiv == 0) {
		if (t_n.prev == parentID) bisector_neighbors[target].prev = selfID;
		if (t_n.next == parentID) bisector_neighbors[target].next = selfID;
		if (t_n.twin == parentID) bisector_neighbors[target].twin = selfID;
	}
	if (subdiv == center_split) {
		uint prop = bisector_data[target].propagationID;
		auto p_n = bisector_neighbors[prop];
		if (t_n.twin == parentID) bisector_neighbors[target].twin = selfID;
		if (p_n.twin == parentID) bisector_neighbors[prop].twin = selfID;
	}
	if (subdiv == (center_split|right_split)) {
		bisector_neighbors[sib1].twin = selfID;
	}
	if (subdiv == (center_split|left_split)) {
		bisector_neighbors[target].twin = selfID;
	}
	bisector.problem_neighbor = ~0u;
	bisector.state = unchanged_element;
	bisector_data[selfID] = bisector;
}
}

@namespace simplify {
//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
void
main ()
{
	if (threadID > queue.count) {
		return;
	}
	uint selfID = queue.entries[threadID];
	ulong heapID = bisector_ids[selfID];	// ALWAYS EVEN (only even queued)
	uint depth = heap_depth (heapID);
	uint pairID = bisector_neighbors[selfID].prev;
	if (heap_depth (bisector_ids[pairID]) != depth
		|| bisector_data[pairID].state != simplify_element) {
		return;
	}
	uint twin_lo = bisector_neighbors[pairID].prev;
	uint twin_hi = bisector_neighbors[selfID].next;
	if (twin_lo != ~0u) {
		ulong loID = bisector_ids[twin_lo];
		if (heapID > loID) {
			return;
		}
		ulong hiID = bisector_ids[twin_hi];
		if (heap_depth (loID) != depth || heap_depth (hiID) != depth) {
			return;
		}
		if (bisector_data[twin_lo].state != simplify_element
			|| bisector_data[twin_hi].state != simplify_element) {
			return;
		}
	}
	queue_id (simplification.queue, selfID);
}
}

@namespace simplification {
//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
void
main ()
{
	if (threadID > queue.count) {
		return;
	}
	uint selfID = queue.entries[threadID];

	auto self = bisector_data[selfID];
	auto sn = bisector_neighbors[selfID];

	uint pairID = sn.prev;
	auto pair = bisector_data[pairID];
	auto pn = bisector_neighbors[pairID];

	uint twin_loID = pn.prev;
	uint twin_hiID = sn.next;

	bisector_ids[selfID] /= 2;
	bisector_ids[pairID] = 0;

	bisector_neighbors[selfID] = { sn.twin, pn.twin, twin_loID };

	self.propagationID = pairID;
	self.problem_neighbor = pn.twin;
	self.state = merged_element;
	self.modified = true;
	bisector_data[selfID] = self;

	if (self.problem_neighbor != ~0u) {
		queue_id (propagate.queue[1], selfID);
	}

	pair.state = merged_element;
	pair.visible = 0;
	pair.modified = 0;
	bisector_data[pairID] = pair;

	cbt.set_bit_buffer (pairID, false);

	if (twin_loID != ~0u) {
		bisector_ids[twin_loID] /= 2;
		bisector_ids[twin_hiID] = 0;

		auto twin_lo = bisector_data[twin_loID];
		auto ln = bisector_neighbors[twin_loID];
		auto twin_hi = bisector_data[twin_hiID];
		auto hn = bisector_neighbors[twin_hiID];

		bisector_neighbors[selfID] = { ln.twin, hn.twin, selfID };

		twin_lo.propagationID = twin_hiID;
		twin_lo.problem_neighbor = hn.twin;
		twin_lo.state = merged_element;
		twin_lo.modified = true;
		bisector_data[selfID] = twin_lo;

		if (twin_lo.problem_neighbor != ~0u) {
			queue_id (propagate.queue[1], twin_loID);
		}

		twin_hi.state = merged_element;
		twin_hi.visible = 0;
		twin_hi.modified = 0;
		bisector_data[twin_hiID] = twin_hi;

		cbt.set_bit_buffer (twin_hiID, false);
	}
}
}

@namespace propagate.simplify {
void
update_neighbor (uint neighborID, uint deleted, uint selfID)
{
	auto nn = bisector_neighbors[neighborID];
	if (nn.prev == deleted) bisector_neighbors[neighborID].prev = selfID;
	if (nn.next == deleted) bisector_neighbors[neighborID].next = selfID;
	if (nn.twin == deleted) bisector_neighbors[neighborID].twin = selfID;
}

//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
void
main ()
{
	if (threadID > queue[1].count) {
		return;
	}
	uint selfID = queue[1].entries[threadID];
	auto self = bisector_data[selfID];
	uint deleted = self.propagationID;
	uint neighborID = self.problem_neighbor;

	auto neighbor = bisector_data[neighborID];

	if (neighbor.state != merged_element) {
		update_neighbor (neighborID, deleted, selfID);
	} else {
		if (!bisector_ids[neighborID]) {
			neighborID = bisector_neighbors[neighborID].next;
		}
		update_neighbor (neighborID, deleted, selfID);
	}
	self.problem_neighbor = ~0u;
	bisector_data[selfID] = self;
}
}

@namespace indexation {
//[shader(GLCompute, LocalSize=[WORKGROUP_SIZE,1,1])]
void
main ()
{
	uint selfID = threadID;
	if (!bisector_ids[selfID]) {
		return;
	}

	uint slot;
	slot = atomicAdd (draw.indirect[0].vertexCount, 3);
	draw.bisector_indices[slot / 3] = selfID;

	auto self = bisector_data[selfID];

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

#define RUP(x, m) (((x) + (m) - 1) / (m))

//[shader(GLCompute, LocalSize=[1,1,1])]
void
prepare ()
{
	uint vertexCount = draw.indirect[0].vertexCount;
	dispatch[0] = {RUP (vertexCount / 3, WORKGROUP_SIZE), 1, 1};
	dispatch[1] = {RUP (vertexCount * 4 / 3, WORKGROUP_SIZE), 1, 1};
	dispatch[2] = {RUP (draw.modified_bisector_count, WORKGROUP_SIZE), 1, 1};
	draw.bisector_count = vertexCount / 3;
}
}

@namespace test {
@overload void
dispatch (void (*shader)(), uvec3 counts)
{
	for (uint z = 0; z < counts.z; z++) {
		for (uint y = 0; y < counts.y; y++) {
			for (uint x = 0; x < counts.x; x++) {
				GlobalInvocationId = {x, y, z};
				shader ();
			}
		}
	}
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
	printf ("%d\n", count);
	for (uint i = 0; i < count; i++) {
		uint selfID = cbt.decode_bit (i);
		ulong heapID = bisector_ids[selfID];
		auto n = bisector_neighbors[selfID];
		printf ("i:%-3d s:%-3d h:%-4ld p:%-3d n:%-3d t:%-3d %v %v %v\n",
				i, selfID, heapID, n.prev, n.next, n.twin,
				draw.vertex_buffer[3 * selfID + 0],
				draw.vertex_buffer[3 * selfID + 1],
				draw.vertex_buffer[3 * selfID + 2]);
	}
}

}

int
main()
{
	test.initialize_bisectors ();
	printf ("base_depth: %ld\n", base_depth);
	cbt.dump ();
	test.dump ();
	return 0;
}
