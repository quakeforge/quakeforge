#ifndef __terrain_h
#define __terrain_h

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

#endif//__terrain_h
