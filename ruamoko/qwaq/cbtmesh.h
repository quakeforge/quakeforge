#ifndef __cbtmesh_h
#define __cbtmesh_h

#include <QF/qfmodel.h>

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

typedef struct QueueData {
	queue_t     split;
	queue_t     simplify;
	queue_t     allocate;
	queue_t     propagate[2];
	queue_t     simplification;
} QueueData;

typedef struct MeshData {
	struct {
		uint       *heap;
		uint        depth;
		uint        max_count;
	}           cbt;
	uint       *bisector_map;
	bisector_t *bisector_data;
	neighbors_t *neighbors[2];
	ulong      *bisector_ids;
	struct {
		halfedge_t *halfedges;
		vec3       *vertices;
		uint        depth;
	}           base;
	struct {
		DispatchIndirect dispatch[3];
		DrawIndirect indirect[2];
		uint        modified_bisector_count;
		uint        bisector_count;
		uint       *bisector_indices;
		uint       *visible_indices;
		uint       *modified_indices;
		vec3       *vertices;
	}           draw;
	QueueData   queues;
	MemoryBuffer memory;

	DispatchIndirect indirect[2];
} MeshData;

typedef struct CameraData {
	mat4 ProjView;
	vec4 frustum_planes[4];
	vec3 camera_pos;
	float pad0;
	vec3 camera_fwd;
	float pad1;
} CameraData;

typedef struct NeighborInds {
	uint cur;
	uint next;
} NeighborInds;

#endif//__cbtmesh_h
