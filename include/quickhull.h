#ifndef __quickhull_h
#define __quickhull_h

#include "QF/ecs.h"
#include "QF/qfmodel.h"
#include "QF/set.h"

typedef struct {
	vec3_t      v;
	vec3_t      m;
	int         a;
	int         b;
} qh_edge_t;

typedef struct qhullctx_s {
	//SOA
	vec4f_t    *verts;
	struct {
		vec4f_t    *planes;
		set_t      *outside_sets;
		int        *highest_points;
		float      *heights;
	} faces;
	quarteredge_t *halfedges;//indexed by face * 3
	set_pool_t  set_pool;
	ecs_idpool_t face_ids;
	int         num_verts;
} __attribute__((aligned(16))) qhullctx_t;

size_t quickhull_context_size (int num_verts) __attribute__((const));
void quickhull_init_context (int num_verts, void *memory);
qhullctx_t *quickhull_new_context (int num_verts);
void quickhull_delete_context (qhullctx_t *qhull);
bool quickhull_make_hull (qhullctx_t *qhull);

#endif
