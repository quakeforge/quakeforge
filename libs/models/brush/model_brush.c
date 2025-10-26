/*
	model_brush.c

	model loading and caching

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/time.h>

#include "QF/checksum.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/heapsort.h"
#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/set.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/vid_render.h"
#include "QF/simd/vec4f.h"
#include "QF/thread/schedule.h"

#include "compat.h"
#include "mod_internal.h"

VISIBLE mleaf_t *
Mod_PointInLeaf (vec4f_t p, const mod_brush_t *brush)
{
	qfZoneScoped (true);
	float       d;

	if (!brush || !brush->nodes)
		Sys_Error ("Mod_PointInLeaf: bad brush");

	int         node_id = 0;
	while (1) {
		if (node_id < 0)
			return brush->leafs + ~node_id;
		mnode_t    *node = brush->nodes + node_id;
		d = dotf (p, node->plane)[0];
		node_id = node->children[d < 0];
	}

	return NULL;						// never reached
}

static inline uint32_t
Mod_CompressVis (byte *out, const byte *vis, uint32_t num_vis)
{
	qfZoneScoped (true);
	byte *start = out;
	uint32_t vis_row = (num_vis + 7) >> 3;
	for (uint32_t j = 0; j < vis_row; j++) {
		*out++ = vis[j];
		if (vis[j]) {
			continue;
		}
		int rep = 1;
		for (j++; j < vis_row; j++) {
			if (vis[j] || rep == 255) {
				break;
			} else {
				rep++;
			}
		}
		*out++ = rep;
		j--;
	}
	return out - start;
}

static inline void
Mod_DecompressVis_set (const byte *in, uint32_t num_vis, byte defvis,
					   set_t *pvs)
{
	qfZoneScoped (true);
	byte       *out = (byte *) pvs->map;
	byte       *start = out;
	int			row, c;

	// Ensure the set repesents visible leafs rather than invisible leafs.
	pvs->inverted = 0;
	row = (num_vis + 7) >> 3;

	if (!in) {							// no vis info, so make all visible
		while (row) {
			*out++ = defvis;
			row--;
		}
		return;
	}

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - start < row);
}

static inline void
Mod_DecompressVis_mix (const byte *in, uint32_t num_vis, byte defvis,
					   set_t *pvs)
{
	qfZoneScoped (true);
	byte       *out = (byte *) pvs->map;
	byte       *start = out;
	int			row, c;

	//FIXME should pvs->inverted be checked and the vis bits used to remove
	// set bits?
	row = (num_vis + 7) >> 3;

	if (!in) {							// no vis info, so make all visible
		while (row) {
			*out++ |= defvis;
			row--;
		}
		return;
	}

	do {
		if (*in) {
			*out++ |= *in++;
			continue;
		}

		c = in[1];
		in += 2;
		out += c;
	} while (out - start < row);
}

VISIBLE void
Mod_LeafPVS_set (const mleaf_t *leaf, const mod_brush_t *brush, byte defvis,
				 set_t *out)
{
	qfZoneScoped (true);
	unsigned    numvis = brush->visleafs;
	unsigned    excess = SET_SIZE (numvis) - numvis;

	set_expand (out, numvis);
	if (leaf == brush->leafs) {
		memset (out->map, defvis, SET_WORDS (out) * sizeof (*out->map));
		out->map[SET_WORDS (out) - 1] &= (~SET_ZERO) >> excess;
		return;
	}
	Mod_DecompressVis_set (leaf->compressed_vis, brush->visleafs, defvis, out);
	out->map[SET_WORDS (out) - 1] &= (~SET_ZERO) >> excess;
}

VISIBLE void
Mod_LeafPVS_mix (const mleaf_t *leaf, const mod_brush_t *brush, byte defvis,
				 set_t *out)
{
	qfZoneScoped (true);
	unsigned    numvis = brush->visleafs;
	unsigned    excess = SET_SIZE (numvis) - numvis;

	set_expand (out, numvis);
	if (leaf == brush->leafs) {
		byte       *o = (byte *) out->map;
		for (int i = SET_WORDS (out) * sizeof (*out->map); i-- > 0; ) {
			*o++ |= defvis;
		}
		out->map[SET_WORDS (out) - 1] &= (~SET_ZERO) >> excess;
		return;
	}
	Mod_DecompressVis_mix (leaf->compressed_vis, brush->visleafs, defvis, out);
	out->map[SET_WORDS (out) - 1] &= (~SET_ZERO) >> excess;
}

// BRUSHMODEL LOADING =========================================================

//FIXME SLOW! However, it doesn't seem to be a big issue. Leave alone?
static void
mod_unique_miptex_name (texture_t **textures, texture_t *tx, int ind)
{
	qfZoneScoped (true);
	constexpr const int maxlen = sizeof (tx->name) - 1;
	char        name[maxlen + 1];
	int         num = 0, i;
	const char *tag;

	strncpy (name, tx->name, maxlen);
	name[maxlen] = 0;
	do {
		for (i = 0; i < ind; i++)
			if (textures[i] && !strcmp (textures[i]->name, tx->name))
				break;
		if (i == ind)
			break;
		tag = va ("~%x", num++);
		// safe because name is known to be truncated and no bigger than
		// tx->name
		strcpy (tx->name, name);
		if (strlen (name) + strlen (tag) <= maxlen)
			strcat (tx->name, tag);
		else
			strcpy (tx->name + maxlen - strlen (tag), tag);
	} while (1);
}

typedef struct {
	bool        alt;
	int         index;
} texanim_t;

static texanim_t
get_texanim (const texture_t *tx)
{
	qfZoneScoped (true);
	int code = tx->name[1];
	if (code >= 'a' && code <= 'z') {
		// convert to uppercase, avoiding toupper (table lookup,
		// localization issues, etc)
		code = code - ('a' - 'A');
	}
	if (code >= '0' && code <= '9') {
		return (texanim_t) {
			.alt = false,
			.index = code - '0',
		};
	} else if (code >= 'A' && code <= 'J') {
		return (texanim_t) {
			.alt = true,
			.index = code - 'A',
		};
	} else {
		Sys_Error ("Bad animating texture %s", tx->name);
	}
}


static void
Mod_LoadTextures (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	dmiptexlump_t  *m;
	int				pixels;
	miptex_t	   *mt;
	texture_t	   *tx, *tx2;

	if (!bsp->texdatasize) {
		brush->textures = NULL;
		return;
	}
	m = (dmiptexlump_t *) bsp->texdata;

	brush->numtextures = m->nummiptex;
	size_t size = sizeof (texture_t[m->nummiptex]);
	brush->textures = Hunk_AllocName (0, size, mod->name);

	for (uint32_t i = 0; i < m->nummiptex; i++) {
		if (m->dataofs[i] == ~0u)
			continue;
		mt = (miptex_t *) ((byte *) m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (int j = 0; j < MIPLEVELS; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		if ((mt->width & 15) || (mt->height & 15))
			Sys_Error ("Texture %s is not 16 aligned", mt->name);
		pixels = mt->width * mt->height / 64 * 85;
		tx = Hunk_AllocName (0, sizeof (texture_t) + pixels, mod->name);

		brush->textures[i] = tx;

		memcpy (tx->name, mt->name, sizeof (tx->name));
		mod_unique_miptex_name (brush->textures, tx, i);
		tx->width = mt->width;
		tx->height = mt->height;
		for (int j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] =
				mt->offsets[j] + sizeof (texture_t) - sizeof (miptex_t);
		// the pixels immediately follow the structures
		memcpy (tx + 1, mt + 1, pixels);

		if (!strncmp (mt->name, "sky", 3))
			brush->skytexture = tx;
	}
	if (mod_funcs && mod_funcs->Mod_ProcessTexture) {
		size_t      render_size = mod_funcs->texture_render_size;
		byte       *render_data = 0;
		if (render_size) {
			render_data = Hunk_AllocName (0, m->nummiptex * render_size,
										  mod->name);
		}
		for (uint32_t i = 0; i < m->nummiptex; i++) {
			if (!(tx = brush->textures[i])) {
				continue;
			}
			tx->render = render_data;
			render_data += render_size;
			mod_funcs->Mod_ProcessTexture (mod, tx);
		}
		// signal the end of the textures
		mod_funcs->Mod_ProcessTexture (mod, 0);
	}

	// sequence the animations
	for (uint32_t i = 0; i < m->nummiptex; i++) {
		tx = brush->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;					// already sequenced

		int             max[2] = {0, 0};
		texture_t	   *anims[2][10] = {};

		// find the number of frames in the animation
		auto texanim = get_texanim (tx);
		anims[texanim.alt][texanim.index] = tx;
		max[texanim.alt] = texanim.index + 1;

		for (uint32_t j = i + 1; j < m->nummiptex; j++) {
			tx2 = brush->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strncmp (tx2->name + 2, tx->name + 2, sizeof (tx->name) - 2))
				continue;

			texanim = get_texanim (tx2);
			anims[texanim.alt][texanim.index] = tx2;
			if (texanim.index + 1 > max[texanim.alt]) {
				max[texanim.alt] = texanim.index + 1;
			}
		}

		// link them all together
		for (int k = 0; k < 2; k++) {
			for (int j = 0; j < max[k]; j++) {
				tx2 = anims[k][j];
				if (!tx2)
					Sys_Error ("Missing frame %i of %s", j, tx->name);
				tx2->anim_total = max[k] * ANIM_CYCLE;
				tx2->anim_min = j * ANIM_CYCLE;
				tx2->anim_max = (j + 1) * ANIM_CYCLE;
				tx2->anim_next = anims[k][(j + 1) % max[k]];
				if (max[1 - k]) {
					tx2->alternate_anims = anims[1 - k][0];
				}
			}
		}
	}
}

static int
leaf_compare (const void *_la, const void *_lb)
{
	qfZoneScoped (true);
	const leafvis_t *la = _la;
	const leafvis_t *lb = _lb;
	if (la->visoffs == lb->visoffs) {
		return la->leafnum - lb->leafnum;
	}
	return la->visoffs - lb->visoffs;
}

typedef struct {
	set_t      *base_pvs;
	set_t      *worker_pvs;
	uint32_t   *vis_rows;
	uint32_t    num_clusters;

	set_pool_t *set_pools;

	const task_t     *tasks;
	const bsp_t      *bsp;
	const mod_brush_t *brush;

	pthread_mutex_t fence_mut;
	pthread_cond_t fence_cond;
	bool        done;
} cluster_data_t;

static void
cluster_vis_wait (task_t *task, int worker_id)
{
	qfZoneScoped (true);
	auto cluster_data = (cluster_data_t *) task->data;

	pthread_mutex_lock (&cluster_data->fence_mut);
	cluster_data->done = true;
	pthread_cond_signal (&cluster_data->fence_cond);
	pthread_mutex_unlock (&cluster_data->fence_mut);
}

static void
cluster_vis_task (task_t *task, int worker_id)
{
	qfZoneScoped (true);
	auto cluster_data = (cluster_data_t *) task->data;
	auto base_pvs = cluster_data->base_pvs;
	auto vis = &cluster_data->worker_pvs[worker_id];
	auto set_pool = &cluster_data->set_pools[worker_id];
	auto vis_rows = cluster_data->vis_rows;

	auto bsp = cluster_data->bsp;
	auto brush = cluster_data->brush;
	auto cluster_map = brush->cluster_map;
	auto leaf_map = brush->leaf_map;
	int i = task - cluster_data->tasks;

	auto leaf = &bsp->leafs[leaf_map[i].first_leaf + 1];
	byte *visdata = nullptr;
	if (leaf->visofs >= 0) {
		visdata = bsp->visdata + leaf->visofs;
	}
	Mod_DecompressVis_set (visdata, brush->visleafs, 0xff, vis);

	set_empty (&base_pvs[i]);
	for (auto iter = set_first_r (set_pool, vis); iter;
		 iter = set_next_r (set_pool, iter)) {
		set_add (&base_pvs[i], cluster_map[iter->element]);
	}
	vis_rows[i] = Mod_CompressVis ((byte *) base_pvs[i].map,
								   (byte *) base_pvs[i].map,
								   cluster_data->num_clusters);
}

static void
init_timeout (struct timespec *timeout, int64_t time)
{
	#define SEC 1000000000L
	struct timeval now;
	gettimeofday(&now, 0);
	timeout->tv_sec = now.tv_sec;
	timeout->tv_nsec = now.tv_usec * 1000L + time;
	if (timeout->tv_nsec >= SEC) {
		timeout->tv_sec += timeout->tv_nsec / SEC;
		timeout->tv_nsec %= SEC;
	}
}


static void
cluster_wait_complete (cluster_data_t *cluster_data)
{
	struct timespec timeout;
	init_timeout (&timeout, 50 * 1000000);
	pthread_mutex_lock (&cluster_data->fence_mut);
	while (!cluster_data->done) {
		pthread_cond_timedwait (&cluster_data->fence_cond,
								&cluster_data->fence_mut, &timeout);
		//pthread_cond_wait (&fence_cond, &fence_mut);
		init_timeout (&timeout, 50 * 1000000);
	}
	pthread_mutex_unlock (&cluster_data->fence_mut);
}

static void
Mod_LoadVisibility (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	if (!bsp->visdatasize) {
		brush->visdata = NULL;
		return;
	}
	brush->visdata = Hunk_AllocName (0, bsp->visdatasize, mod->name);
	memcpy (brush->visdata, bsp->visdata, bsp->visdatasize);

	int64_t start = Sys_LongTime ();

	uint32_t num_leafs = bsp->models[0].visleafs;
	uint32_t num_clusters = 1;
	leafvis_t *leafvis = Hunk_TempAlloc (0, sizeof (leafvis_t[num_leafs]));
	bool sorted = true;

	for (uint32_t i = 0; i < num_leafs; i++) {
		leafvis[i].visoffs = bsp->leafs[i + 1].visofs;
		leafvis[i].leafnum = i;
		if (i > 0) {
			num_clusters += leafvis[i].visoffs != leafvis[i - 1].visoffs;
			if (leafvis[i].visoffs < leafvis[i - 1].visoffs) {
				sorted = false;
			}
		}
	}
	if (!sorted) {
		Sys_Error ("Mod_LoadVisibility: scrambled leafs not fully implemented");
		heapsort (leafvis, num_leafs, sizeof (leafvis_t), leaf_compare);
		num_clusters = 1;
		for (uint32_t i = 1; i < num_leafs; i++) {
			num_clusters += leafvis[i].visoffs != leafvis[i - 1].visoffs;
		}
	}

	printf ("leafs   : %u\n", num_leafs);
	printf ("clusters: %u\n", num_clusters);
	if (num_clusters == num_leafs) {
		// no clusters to reconstruct
		return;
	}

	size_t size = sizeof (leafmap_t[num_clusters])
				+ sizeof (uint32_t[num_leafs])
				+ sizeof (uint32_t[num_clusters]);
	brush->leaf_map = Hunk_AllocName (0, size, mod->name);
	brush->cluster_map = (uint32_t *) &brush->leaf_map[num_clusters];
	brush->cluster_offs = (uint32_t *) &brush->cluster_map[num_leafs];
	leafmap_t *leafmap = brush->leaf_map;
	uint32_t *leafcluster = brush->cluster_map;

	auto lm = leafmap;
	uint32_t offs = leafvis[0].visoffs;
	for (uint32_t i = 0; i < num_leafs; i++) {
		if (leafvis[i].visoffs != offs) {
			lm++;
			lm->first_leaf = i;
			offs = leafvis[i].visoffs;
		}
		leafcluster[leafvis[i].leafnum] = lm - leafmap;
		lm->num_leafs++;
	}

	brush->visleafs = bsp->models[0].visleafs;
	uint32_t cluster_visbytes = (num_clusters + 7) / 8;
	uint32_t leaf_visbytes = (num_leafs + 7) / 8;
	int num_workers = wssched_worker_count (brush_ctx->sched);
	cluster_visbytes = (cluster_visbytes * 3) / 2 + 1;
	size = sizeof (set_t[num_clusters])
		 + sizeof (set_t[num_workers])
		 + sizeof (set_pool_t[num_workers])
		 + sizeof (task_t[1])
		 + sizeof (task_t[num_clusters])
		 + sizeof (byte[cluster_visbytes * num_clusters])
		 + sizeof (byte[leaf_visbytes * num_workers]);
	auto base_pvs = (set_t *) Hunk_TempAlloc (0, size);
	auto worker_pvs = &base_pvs[num_clusters];
	auto set_pools = (set_pool_t *) &worker_pvs[num_workers];
	auto wait_task = (task_t *) &set_pools[num_workers];
	auto cluster_tasks = &wait_task[1];
	auto cluster_visdata = (byte *) &cluster_tasks[num_clusters];

	uint32_t vis_rows[num_clusters];
	cluster_data_t cluster_data = {
		.base_pvs = base_pvs,
		.worker_pvs = &base_pvs[num_clusters],
		.set_pools = set_pools,
		.vis_rows = vis_rows,
		.num_clusters = num_clusters,

		.tasks = cluster_tasks,
		.bsp = bsp,
		.brush = brush,

		.fence_mut = PTHREAD_MUTEX_INITIALIZER,
		.fence_cond = PTHREAD_COND_INITIALIZER,
	};

	*wait_task = (task_t) {
		.dependency_count = num_clusters,
		.execute = cluster_vis_wait,
		.data = &cluster_data,
	};
	for (int i = 0; i < num_workers; i++) {
#define vis_alloc(x) (set_bits_t *) (cluster_visdata \
									 + num_clusters * cluster_visbytes \
									 + i * leaf_visbytes)
		cluster_data.worker_pvs[i] =
			(set_t) SET_STATIC_INIT (num_leafs - 1, vis_alloc);
#undef vis_alloc
		set_pool_init (&cluster_data.set_pools[i]);
	};

	uint32_t total_bytes = 0;
	task_t *cluster_task_set[num_clusters];
	for (uint32_t i = 0; i < num_clusters; i++) {
#define vis_alloc(x) (set_bits_t *) (cluster_visdata + i * cluster_visbytes)
		cluster_data.base_pvs[i] =
			(set_t) SET_STATIC_INIT (num_clusters - 1, vis_alloc);
#undef vis_alloc
		cluster_tasks[i] = (task_t) {
			.child_count = 1,
			.children = &wait_task,
			.execute = cluster_vis_task,
			.data = &cluster_data,
		};
		cluster_task_set[i] = &cluster_tasks[i];
	};
	wssched_insert (brush_ctx->sched, num_clusters, cluster_task_set);

	cluster_wait_complete (&cluster_data);

	for (int i = 0; i < num_workers; i++) {
		set_pool_clear (&cluster_data.set_pools[i]);
	};

	for (uint32_t i = 0; i < num_clusters; i++) {
		total_bytes += vis_rows[i];
	}

	brush->vis_clusters = num_clusters;
	brush->cluster_vis = Hunk_AllocName (0, total_bytes, mod->name);
	uint32_t offset = 0;
	for (uint32_t i = 0; i < num_clusters; i++) {
		memcpy (brush->cluster_vis + offset, base_pvs[i].map, vis_rows[i]);
		brush->cluster_offs[i] = offset;
		offset += vis_rows[i];
	}

	int64_t end = Sys_LongTime ();
	printf ("Mod_LoadVisibility: %'"PRIi64"\n", end - start);
}

static void
Mod_LoadEntities (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	if (!bsp->entdatasize) {
		brush->entities = NULL;
		return;
	}
	brush->entities = Hunk_AllocName (0, bsp->entdatasize, mod->name);
	memcpy (brush->entities, bsp->entdata, bsp->entdatasize);
}

static void
Mod_LoadVertexes (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	dvertex_t  *in;
	int         count, i;
	mvertex_t  *out;

	in = bsp->vertexes;
	count = bsp->numvertexes;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	brush->vertexes = out;
	brush->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
		VectorCopy (in->point, out->position);
}

static void
Mod_LoadSubmodels (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	dmodel_t   *in, *out;
	int         count, i, j;

	in = bsp->models;
	count = bsp->nummodels;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	brush->submodels = out;
	brush->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		static vec3_t offset = {1, 1, 1};
		// spread the mins / maxs by a pixel
		VectorSubtract (in->mins, offset, out->mins);
		VectorAdd (in->maxs, offset, out->maxs);
		VectorCopy (in->origin, out->origin);
		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = in->headnode[j];
		out->visleafs = in->visleafs;
		out->firstface = in->firstface;
		out->numfaces = in->numfaces;
	}

	out = brush->submodels;

	if (out->visleafs > 8192)
		Sys_MaskPrintf (SYS_warn,
						"%i visleafs exceeds standard limit of 8192.\n",
						out->visleafs);
}

static void
Mod_LoadEdges (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	dedge_t    *in;
	int         count, i;
	medge_t    *out;

	in = bsp->edges;
	count = bsp->numedges;
	out = Hunk_AllocName (0, (count + 1) * sizeof (*out), mod->name);

	brush->edges = out;
	brush->numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = in->v[0];
		out->v[1] = in->v[1];
	}
}

static void
Mod_LoadTexinfo (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	float       len1, len2;
	unsigned    count, miptex, i, j;
	mtexinfo_t *out;
	texinfo_t  *in;

	in = bsp->texinfo;
	count = bsp->numtexinfo;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	brush->texinfo = out;
	brush->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 4; j++) {
			out->vecs[0][j] = in->vecs[0][j];
			out->vecs[1][j] = in->vecs[1][j];
		}
		len1 = VectorLength (out->vecs[0]);
		len2 = VectorLength (out->vecs[1]);

		len1 = (len1 + len2) / 2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = in->miptex;
		out->flags = in->flags;

		if (!brush->textures) {
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		} else {
			if (miptex >= brush->numtextures)
				Sys_Error ("miptex >= brush->numtextures");
			out->texture = brush->textures[miptex];
			if (!out->texture) {
				out->texture = r_notexture_mip;	// texture not found
				out->flags = 0;
			}
		}
	}
}

/*
	CalcSurfaceExtents

	Fills in s->texturemins[] and s->extents[]
*/
static void
CalcSurfaceExtents (model_t *mod, msurface_t *s)
{
	qfZoneScoped (true);
	float		mins[2], maxs[2], val;
	int			e, i, j;
	int			bmins[2], bmaxs[2];
	mtexinfo_t *tex;
	mvertex_t  *v;
	mod_brush_t *brush = &mod->brush;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0; i < s->numedges; i++) {
		e = brush->surfedges[s->firstedge + i];
		if (e >= 0)
			v = &brush->vertexes[brush->edges[e].v[0]];
		else
			v = &brush->vertexes[brush->edges[-e].v[1]];

		for (j = 0; j < 2; j++) {
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++) {
		bmins[i] = floor (mins[i] / 16);
		bmaxs[i] = ceil (maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		// FIXME even 2000 is really too small, need a saner test
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 2000)
			Sys_Error ("Bad surface extents: %d %x %d %d", i, tex->flags,
					   s->extents[i], LongSwap (s->extents[i]));
	}
}

static void
Mod_LoadFaces (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;

	dface_t    *in;
	int			count, planenum, side, surfnum, i;
	msurface_t *out;

	in = bsp->faces;
	count = bsp->numfaces;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	if (count > 32767) {
		Sys_MaskPrintf (SYS_warn,
						"%i faces exceeds standard limit of 32767.\n", count);
	}

	brush->surfaces = out;
	brush->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = in->firstedge;
		out->numedges = in->numedges;
		out->flags = 0;

		planenum = in->planenum;
		side = in->side;
		if (side)
			out->flags |= SURF_PLANEBACK;

		out->plane = brush->planes + planenum;

		out->texinfo = brush->texinfo + in->texinfo;

		CalcSurfaceExtents (mod, out);

		// lighting info

		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = in->lightofs;
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = brush->lightdata + (i * brush->lightmap_bytes);

		// set the drawing flags flag
		if (!out->texinfo->texture) {
			// avoid crashing on null textures (which do exist)
			continue;
		}

		if (!strncmp (out->texinfo->texture->name, "sky", 3)) {	// sky
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			if (brush_ctx->sky_divide) {
				if (mod_funcs && mod_funcs->Mod_SubdivideSurface) {
					mod_funcs->Mod_SubdivideSurface (mod, out);
				}
			}
			continue;
		}

		switch (out->texinfo->texture->name[0]) {
			case '*':	// turbulent
				out->flags |= (SURF_DRAWTURB
							   | SURF_DRAWTILED
							   | SURF_LIGHTBOTHSIDES);
				for (i = 0; i < 2; i++) {
					out->extents[i] = 16384;
					out->texturemins[i] = -8192;
				}
				if (mod_funcs && mod_funcs->Mod_SubdivideSurface) {
					// cut up polygon for warps
					mod_funcs->Mod_SubdivideSurface (mod, out);
				}
				break;
			case '{':
				out->flags |= SURF_DRAWALPHA;
				break;
		}
	}
}

static void
Mod_SetParent (mod_brush_t *brush, int node_id, int parent_id)
{
	qfZoneScoped (true);
	if (node_id < 0) {
		brush->leaf_parents[~node_id] = parent_id;
		return;
	}
	brush->node_parents[node_id] = parent_id;
	mnode_t    *node = brush->nodes + node_id;
	Mod_SetParent (brush, node->children[0], node_id);
	Mod_SetParent (brush, node->children[1], node_id);
}

static int
Mod_PropagateClusters (mod_brush_t *brush, int node_id, int32_t *node_cluster)
{
	qfZoneScoped (true);
	if (node_id < 0) {
		return 0;
	}
	mnode_t    *node = brush->nodes + node_id;
	int c = 0;
	c += Mod_PropagateClusters (brush, node->children[0], node_cluster);
	c += Mod_PropagateClusters (brush, node->children[1], node_cluster);

	int c1 = 0;
	int c2 = 0;
	if (node->children[0] < 0) {
		c1 = ~brush->cluster_map[~node->children[0]];
	} else {
		c1 = node_cluster[node->children[0]];
	}
	if (node->children[1] < 0) {
		c2 = ~brush->cluster_map[~node->children[1]];
	} else {
		c2 = node_cluster[node->children[1]];
	}
	if (c1 == c2 || c2 == -1) {
		node_cluster[node_id] = c1;
	} else if (c1 == -1) {
		node_cluster[node_id] = c2;
	} else {
		node_cluster[node_id] = 0;
	}
	c += node_cluster[node_id] == 0;
	return c;
}

static void
Mod_SetLeafFlags (mod_brush_t *brush)
{
	qfZoneScoped (true);
	for (unsigned i = 0; i < brush->modleafs; i++) {
		int         flags = 0;
		mleaf_t    *leaf = &brush->leafs[i];
		msurface_t **msurf = brush->marksurfaces + leaf->firstmarksurface;
		for (int j = 0; j < leaf->nummarksurfaces; j++) {
			msurface_t *surf = *msurf++;
			flags |= surf->flags;
		}
		brush->leaf_flags[i] = flags;
	}
}

static void
Mod_LoadNodes (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	dnode_t    *in;
	int			count, i, j, p;
	mnode_t    *out;

	in = bsp->nodes;
	count = bsp->numnodes;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	if (count > 32767) {
		Sys_MaskPrintf (SYS_warn,
						"%i nodes exceeds standard limit of 32767.\n", count);
	}

	brush->nodes = out;
	brush->numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = in->mins[j];
			out->minmaxs[3 + j] = in->maxs[j];
		}

		plane_t    *plane = brush->planes + in->planenum;
		out->plane = loadvec3f (plane->normal);
		out->plane[3] = -plane->dist;
		out->type = plane->type;
		out->signbits = plane->signbits;

		out->firstsurface = in->firstface;
		out->numsurfaces = in->numfaces;

		for (j = 0; j < 2; j++) {
			p = in->children[j];
			// this check is for extended bsp 29 files
			if (p >= 0) {
				out->children[j] = p;
			} else {
				if ((unsigned) ~p < brush->modleafs) {
					out->children[j] = p;
				} else {
					Sys_Printf ("Mod_LoadNodes: invalid leaf index %i "
								"(file has only %i leafs)\n", ~p,
								brush->modleafs);
					//map it to the solid leaf
					out->children[j] = ~0;
				}
			}
		}
	}

	size_t      size = (brush->modleafs + brush->numnodes) * sizeof (int32_t);
	size += brush->modleafs * sizeof (int);
	brush->node_parents = Hunk_AllocName (0, size, mod->name);
	brush->leaf_parents = brush->node_parents + brush->numnodes;
	brush->leaf_flags = (int *) (brush->leaf_parents + brush->modleafs);
	Mod_SetParent (brush, 0, -1);	// sets nodes and leafs
	Mod_SetLeafFlags (brush);
}

static void
Mod_LoadLeafs (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	dleaf_t    *in;
	int			count, i, j, p;
	mleaf_t    *out;

	in = bsp->leafs;
	count = bsp->numleafs;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	brush->leafs = out;
	brush->modleafs = count;
	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->mins[j] = in->mins[j];
			out->maxs[j] = in->maxs[j];
		}

		p = in->contents;
		out->contents = p;

		out->firstmarksurface = in->firstmarksurface;
		out->nummarksurfaces = in->nummarksurfaces;

		p = in->visofs;
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = brush->visdata + p;
		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
	}
}

static void
Mod_LoadClipnodes (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	dclipnode_t *in;
	dclipnode_t *out;
	hull_t		*hull;
	int         count, i;

	in = bsp->clipnodes;
	count = bsp->numclipnodes;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	if (count > 32767) {
		Sys_MaskPrintf (SYS_warn,
						"%i clilpnodes exceeds standard limit of 32767.\n",
						count);
	}

	brush->clipnodes = out;
	brush->numclipnodes = count;

	hull = &brush->hulls[1];
	brush->hull_list[1] = hull;
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brush->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &brush->hulls[2];
	brush->hull_list[2] = hull;
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brush->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = in->planenum;
		if (out->planenum >= brush->numplanes)
			Sys_Error ("Mod_LoadClipnodes: planenum out of bounds");
		out->children[0] = in->children[0];
		out->children[1] = in->children[1];
		// these checks are for extended bsp 29 files
		if (out->children[0] >= count)
			out->children[0] -= 65536;
		if (out->children[1] >= count)
			out->children[1] -= 65536;
		if ((out->children[0] >= 0
			 && (out->children[0] < hull->firstclipnode
				 || out->children[0] > hull->lastclipnode))
			|| (out->children[1] >= 0
				&& (out->children[1] < hull->firstclipnode
					|| out->children[1] > hull->lastclipnode)))
			Sys_Error ("Mod_LoadClipnodes: bad node number");
	}
}

/*
	Mod_MakeHull0

	Replicate the drawing hull structure as a clipping hull
*/
static void
Mod_MakeHull0 (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	dclipnode_t *out;
	hull_t		*hull;
	int			 count, i, j;
	mnode_t		*in;

	hull = &brush->hulls[0];
	brush->hull_list[0] = hull;

	in = brush->nodes;
	count = brush->numnodes;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count - 1;
	hull->planes = brush->planes;

	for (i = 0; i < count; i++, out++, in++) {
		out->planenum = bsp->nodes[i].planenum;
		for (j = 0; j < 2; j++) {
			int         child_id = in->children[j];
			if (child_id < 0) {
				out->children[j] = bsp->leafs[~child_id].contents;
			} else {
				out->children[j] = child_id;
			}
		}
	}
}

static void
Mod_LoadMarksurfaces (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	unsigned    count, i, j;
	msurface_t **out;
	uint32_t    *in;

	in = bsp->marksurfaces;
	count = bsp->nummarksurfaces;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	if (count > 32767) {
		Sys_MaskPrintf (SYS_warn,
						"%i marksurfaces exceeds standard limit of 32767.\n",
						count);
	}

	brush->marksurfaces = out;
	brush->nummarksurfaces = count;

	for (i = 0; i < count; i++) {
		j = in[i];
		if (j >= brush->numsurfaces)
			Sys_Error ("Mod_ParseMarksurfaces: bad surface number");
		out[i] = brush->surfaces + j;
	}
}

static void
Mod_LoadSurfedges (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	int          count, i;
	int32_t     *in;
	int         *out;

	in = bsp->surfedges;
	count = bsp->numsurfedges;
	out = Hunk_AllocName (0, count * sizeof (*out), mod->name);

	brush->surfedges = out;
	brush->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = in[i];
}

static void
Mod_LoadPlanes (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;
	dplane_t   *in;
	int			bits, count, i, j;
	plane_t    *out;

	in = bsp->planes;
	count = bsp->numplanes;
	out = Hunk_AllocName (0, count * 2 * sizeof (*out), mod->name);

	brush->planes = out;
	brush->numplanes = count;

	for (i = 0; i < count; i++, in++, out++) {
		bits = 0;
		for (j = 0; j < 3; j++) {
			out->normal[j] = in->normal[j];
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = in->dist;
		out->type = in->type;
		out->signbits = bits;
	}
}

static void
do_checksums (const bsp_t *bsp, void *_mod)
{
	qfZoneScoped (true);
	int         i;
	model_t    *mod = (model_t *) _mod;
	byte       *base;
	mod_brush_t *brush = &mod->brush;

	base = (byte *) bsp->header;

	// checksum all of the map, except for entities
	brush->checksum = 0;
	brush->checksum2 = 0;
	for (i = 0; i < HEADER_LUMPS; i++) {
		lump_t     *lump = bsp->header->lumps + i;
		int         csum;

		if (i == LUMP_ENTITIES)
			continue;
		csum = Com_BlockChecksum (base + lump->fileofs, lump->filelen);
		brush->checksum ^= csum;

		if (i != LUMP_VISIBILITY && i != LUMP_LEAFS && i != LUMP_NODES)
			brush->checksum2 ^= csum;
	}
}

static void
recurse_draw_tree (mod_brush_t *brush, int node_id, int depth)
{
	if (node_id < 0) {
		if (depth > brush->depth)
			brush->depth = depth;
		return;
	}
	mnode_t    *node = &brush->nodes[node_id];
	recurse_draw_tree (brush, node->children[0], depth + 1);
	recurse_draw_tree (brush, node->children[1], depth + 1);
}

static void
Mod_FindDrawDepth (mod_brush_t *brush)
{
	qfZoneScoped (true);
	brush->depth = 0;
	recurse_draw_tree (brush, 0, 1);
}

static int
Mod_BuildClusterNodes (mod_brush_t *brush, int node_id, int32_t *node_cluster,
					   mnode_t *cluster_nodes, int *cluster_depth, int depth,
					   int *num_nodes)
{
	if (depth > *cluster_depth) {
		*cluster_depth = depth;
	}
	if (node_id < 0) {
		return node_id;
	}
	if (node_cluster[node_id] < 0) {
		return node_cluster[node_id];
	}
	auto node = brush->nodes + node_id;
	int id = (*num_nodes)++;
	cluster_nodes[id] = brush->nodes[node_id];
	int l = Mod_BuildClusterNodes (brush, node->children[0], node_cluster,
								   cluster_nodes, cluster_depth, depth + 1,
								   num_nodes);
	int r = Mod_BuildClusterNodes (brush, node->children[1], node_cluster,
								   cluster_nodes, cluster_depth, depth + 1,
								   num_nodes);
	cluster_nodes[id].children[0] = l;
	cluster_nodes[id].children[1] = r;
	return id;
}

static int
cluster_surf_cmp (const void *_sa, const void *_sb, void *_bsp)
{
	uint32_t surf_id_a = *(uint32_t *) _sa;
	uint32_t surf_id_b = *(uint32_t *) _sb;
	bsp_t *bsp = _bsp;
	auto surf_a = bsp->faces + surf_id_a;
	auto surf_b = bsp->faces + surf_id_b;

	if (surf_a->texinfo == surf_b->texinfo) {
		return surf_id_a - surf_id_b;
	}
	return surf_a->texinfo - surf_b->texinfo;
}

static void
Mod_MakeClusters (mod_brush_ctx_t *brush_ctx)
{
	qfZoneScoped (true);
	auto mod = brush_ctx->mod;
	auto bsp = brush_ctx->bsp;
	auto brush = brush_ctx->brush;

	if (brush->cluster_map) {
		size_t size = sizeof (int32_t[brush->numnodes]);
		int32_t *node_cluster = Hunk_TempAlloc (0, size);
		int num_cluster_nodes = Mod_PropagateClusters (brush, 0, node_cluster);

		brush->cluster_depth = 0;
		size = sizeof (mnode_t[num_cluster_nodes]);
		brush->cluster_nodes = Hunk_AllocName (0, size, mod->name);
		int cluster_node_count = 0;
		Mod_BuildClusterNodes (brush, 0, node_cluster, brush->cluster_nodes,
							   &brush->cluster_depth, 1, &cluster_node_count);
		printf ("num_cluster_nodes: %d %d\n", num_cluster_nodes,
				cluster_node_count);
		if (cluster_node_count != num_cluster_nodes) {
			Sys_Error ("taniwha can't count");
		}

		set_t *seen_surfs = set_new_size (brush->nummarksurfaces);
		int added_surfs = 0;
		int max_surfs = 0;
		for (uint32_t i = 0; i < brush->vis_clusters; i++) {
			set_empty (seen_surfs);
			auto leafmap = brush->leaf_map[i];
			int count = 0;
			for (uint32_t j = 0; j < leafmap.num_leafs; j++) {
				auto leaf = bsp->leafs + leafmap.first_leaf + j;
				for (uint32_t k = 0; k < leaf->nummarksurfaces; k++) {
					int ind = leaf->firstmarksurface + k;
					int surf_id = bsp->marksurfaces[ind];
					if (!set_is_member (seen_surfs, surf_id)) {
						set_add (seen_surfs, surf_id);
						count++;
					}
				}
			}
			added_surfs += count;
			if (count > max_surfs) {
				max_surfs = count;
			}
		}
		printf ("added_surfs: %d %d\n", added_surfs, max_surfs);

		size = sizeof (uint32_t[added_surfs])
			 + sizeof (cluster_t[brush->vis_clusters]);
		brush->cluster_surfs = Hunk_AllocName (0, size, mod->name);
		brush->clusters = (cluster_t *) &brush->cluster_surfs[added_surfs];
		added_surfs = 0;
		for (uint32_t i = 0; i < brush->vis_clusters; i++) {
			//set_empty (seen_surfs);
			auto leafmap = brush->leaf_map[i];
			auto cluster = &brush->clusters[i];
			cluster->firstsurface = added_surfs;
			int count = 0;
			for (uint32_t j = 0; j < leafmap.num_leafs; j++) {
				auto leaf = bsp->leafs + leafmap.first_leaf + j;
				for (uint32_t k = 0; k < leaf->nummarksurfaces; k++) {
					int ind = leaf->firstmarksurface + k;
					int surf_id = bsp->marksurfaces[ind];
					if (!set_is_member (seen_surfs, surf_id)) {
						set_add (seen_surfs, surf_id);
						brush->cluster_surfs[added_surfs++] = surf_id;
						count++;
					}
				}
			}
			cluster->numsurfaces = count;
			if (count > 1) {
				// sort the surface ids by texinfo so drawing can be batched
				// by texture
				heapsort_r (brush->cluster_surfs + cluster->firstsurface,
							count, sizeof (uint32_t), cluster_surf_cmp,
							bsp);
			}
		}
	} else {
		brush->vis_clusters = brush->visleafs;
		brush->cluster_nodes = brush->nodes;
		brush->cluster_depth = brush->depth;
		brush->cluster_vis = brush->visdata;

		int count = brush->visleafs + 1;
		int surfs = brush->nummarksurfaces;
		size_t size = sizeof (leafmap_t[count])		//leaf_map
					+ sizeof (uint32_t[count])		//cluster_map
					+ sizeof (uint32_t[count])		//cluster_offs
					+ sizeof (uint32_t[surfs])		//cluster_surfs
					+ sizeof (cluster_t[count]);	//clusters
		brush->leaf_map = Hunk_AllocName (0, size, mod->name);
		brush->cluster_map = (uint32_t *) &brush->leaf_map[count];
		brush->cluster_offs = (uint32_t *) &brush->cluster_map[count];
		brush->cluster_surfs = (uint32_t *) &brush->cluster_offs[count];
		brush->clusters = (cluster_t *) &brush->cluster_surfs[surfs];
		memcpy (brush->cluster_surfs, bsp->marksurfaces,
				sizeof (uint32_t[surfs]));
		for (int i = 0; i < count; i++) {
			brush->leaf_map[i] = (leafmap_t) {
				.first_leaf = i,
				.num_leafs = 1,
			};
			brush->cluster_map[i] = i;

			auto leaf = brush->leafs + i;
			brush->cluster_offs[i] = leaf->compressed_vis - brush->visdata;

			auto cluster = brush->clusters + i;
			*cluster = (cluster_t) {
				.firstsurface = leaf->firstmarksurface,
				.numsurfaces = leaf->nummarksurfaces,
			};
			if (cluster->numsurfaces > 1) {
				// sort the surface ids by texinfo so drawing can be batched
				// by texture
				heapsort_r (brush->cluster_surfs + cluster->firstsurface,
							cluster->numsurfaces, sizeof (uint32_t),
							cluster_surf_cmp, bsp);
			}
		}
	}
}

void
Mod_LoadBrushModel (model_t *mod, void *buffer, wssched_t *sched)
{
	qfZoneScoped (true);
	dmodel_t   *bm;
	unsigned    i, j;

	mod->type = mod_brush;
	mod->brush = (mod_brush_t) {};

	mod_brush_ctx_t brush_ctx = {
		.mod = mod,
		.brush = &mod->brush,
		.bsp = LoadBSPMem (buffer, qfs_filesize, do_checksums, mod),
		.sched = sched,
	};

	if (mod_funcs && mod_funcs->Mod_BrushContext) {
		mod_funcs->Mod_BrushContext (&brush_ctx);
	}

	// load into heap
	Mod_LoadVertexes (&brush_ctx);
	Mod_LoadEdges (&brush_ctx);
	Mod_LoadSurfedges (&brush_ctx);
	Mod_LoadTextures (&brush_ctx);
	if (mod_funcs && mod_funcs->Mod_LoadLighting) {
		mod_funcs->Mod_LoadLighting (&brush_ctx);
	}
	Mod_LoadPlanes (&brush_ctx);
	Mod_LoadTexinfo (&brush_ctx);
	Mod_LoadFaces (&brush_ctx);
	Mod_LoadMarksurfaces (&brush_ctx);
	Mod_LoadVisibility (&brush_ctx);
	Mod_LoadLeafs (&brush_ctx);
	Mod_LoadNodes (&brush_ctx);
	Mod_LoadClipnodes (&brush_ctx);
	Mod_LoadEntities (&brush_ctx);
	Mod_LoadSubmodels (&brush_ctx);

	Mod_MakeHull0 (&brush_ctx);

	Mod_FindDrawDepth (&mod->brush);
	for (i = 0; i < MAX_MAP_HULLS; i++)
		Mod_FindClipDepth (&mod->brush.hulls[i]);

	Mod_MakeClusters (&brush_ctx);

	BSP_Free(brush_ctx.bsp);

	mod->numframes = 2;					// regular and alternate animation

	// set up the submodels (FIXME: this is confusing)
	for (i = 0; i < mod->brush.numsubmodels; i++) {
		bm = &mod->brush.submodels[i];

		mod->brush.hulls[0].firstclipnode = bm->headnode[0];
		mod->brush.hull_list[0] = &mod->brush.hulls[0];
		for (j = 1; j < MAX_MAP_HULLS; j++) {
			mod->brush.hulls[j].firstclipnode = bm->headnode[j];
			mod->brush.hulls[j].lastclipnode = mod->brush.numclipnodes - 1;
			mod->brush.hull_list[j] = &mod->brush.hulls[j];
		}

		mod->brush.firstmodelsurface = bm->firstface;
		mod->brush.nummodelsurfaces = bm->numfaces;

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->brush.visleafs = bm->visleafs;
		// The bsp file has leafs for all submodes and hulls, so update the
		// leaf count for this model to be the correct number (which is one
		// more than the number of visible leafs)
		mod->brush.modleafs = bm->visleafs + 1;

		if (i < mod->brush.numsubmodels - 1) {
			// duplicate the basic information
			char	name[12];

			snprintf (name, sizeof (name), "*%i", i + 1);
			model_t    *m = Mod_FindName (name);
			*m = *mod;
			strcpy (m->path, name);
			strcpy (m->name, name);
			mod = m;
			// make sure clear is called only for the main model
			m->clear = 0;
			m->data = 0;
		}
	}
}
