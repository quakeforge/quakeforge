/*
	vulkan_lightmap.c

	surface-related refresh code

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Joseph Carter <knghtbrd@debian.org>
	Copyright (C) 2021       Bill Currie <bill@taniwha.org>

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
#include <stdio.h>

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/Vulkan/qf_bsp.h"
#include "QF/Vulkan/qf_lightmap.h"
#include "QF/Vulkan/qf_texture.h"
#include "QF/Vulkan/barrier.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/scrap.h"
#include "QF/Vulkan/staging.h"

#include "QF/scene/entity.h"

#include "compat.h"
#include "r_internal.h"
#include "r_scrap.h"
#include "vid_vulkan.h"

#define ushort uint16_t
#define uint uint32_t
#include "shader/bsp.h"
#define block_size (workgroup_size * 2)

#define s_dynlight (r_refdef.scene->base + scene_dynlight)
#define LUXEL_SIZE 4

typedef struct lmapctx_s {
	vulkan_ctx_t *ctx;
	mod_brush_t *brush;
	rscrap_t    rscrap;
	uint32_t    num_lightmaps;
	uint32_t    lightmap_luxels;
	uint32_t    num_clusters;

	bsp_lightinfo_t *lightinfo;
	bsp_lightsize_t *lightsize;
	bsp_surfinfo_t *surfinfo;
	bsp_lightcache_t *light_cache;
	int16_t    *light_style_values;
	byte       *lightmap_data;
	cluster_t  *light_clusters;
} lmapctx_t;

static void
lmap_model_loop (model_t **models, int num_models,
				 void (*func)(model_t *, lmapctx_t *), lmapctx_t *lmap)
{
	for (int i = 0; i < num_models; i++) {
		model_t    *m = models[i];
		// sub-models are done as part of the main model
		// and non-bsp models don't have surfaces.
		if (!m || m->type != mod_brush || *m->path == '*') {
			continue;
		}
		func (m, lmap);
	}
}

static inline void
add_dynamic_lights (const vec4f_t *transform, msurface_t *surf, vec4f_t *block)
{
	qfZoneScoped (true);
	int         sd, td;
	int         smax, tmax;
	int         s, t;
	mtexinfo_t *tex;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	auto p = surf->plane;
	vec4f_t plane = { VectorExpand (p->normal), -p->dist };
	vec4f_t     entorigin = { 0, 0, 0, 1 };
	if (transform) {
		//FIXME give world entity a transform
		entorigin = transform[3];
	}

	auto dlight_pool = &r_refdef.registry->comp_pools[s_dynlight];
	auto dlight_data = (dlight_t *) dlight_pool->data;
	for (uint32_t i = 0; i < dlight_pool->count; i++) {
		auto dlight = &dlight_data[i];
		if (!(surf->dlightbits[i / 32] & (1 << (i % 32))))
			continue;					// not lit by this light

		vec4f_t lightorigin = dlight->origin - entorigin;
		lightorigin[3] = 1;
		float   rad = dlight->radius;
		vec4f_t dist = dotf (lightorigin, plane);
		dist[3] = 0;
		rad -= fabs (dist[0]);

		float minlight = dlight->minlight;
		if (rad < minlight) {
			continue;
		}
		vec4f_t impact = dlight->origin - dist * plane;

		vec4f_t local = {
			dotf (impact, tex->vecs[0])[0] - surf->texturemins[0],
			dotf (impact, tex->vecs[1])[0] - surf->texturemins[1],
		};

		vec4f_t color = { VectorExpand (dlight->color), 0 };
		color *= dlight->radius / 4096;

		for (t = 0; t < tmax; t++) {
			td = local[1] - t * 16;
			if (td < 0) {
				td = -td;
			}
			for (s = 0; s < smax; s++) {
				sd = local[0] - s * 16;
				if (sd < 0) {
					sd = -sd;
				}
				float d;
				if (sd > td) {
					d = sd + (td >> 1);
				} else {
					d = td + (sd >> 1);
				}
				float l = rad - d;
				if (l > minlight) {
					block[t * smax + s] += l * color;
				}
			}
		}
	}
}

static void
vulkan_create_surf_lightmap (cluster_t *cluster, lmapctx_t *lmap)
{
	qfZoneScoped (true);
	auto brush = lmap->brush;

	for (uint32_t j = 0; j < cluster->count; j++) {
		uint32_t surfind = brush->cluster_surfs[cluster->first + j];
		msurface_t *surf = brush->surfaces + surfind;
		surf->lightpic = nullptr;     // paranoia
		lmap->num_lightmaps++;
		if (surf->flags & SURF_DRAWTURB) {
			continue;
		}
		if (surf->flags & SURF_DRAWSKY) {
			continue;
		}

		int         smax, tmax;

		smax = (surf->extents[0] >> 4) + 1;
		tmax = (surf->extents[1] >> 4) + 1;

		int i;
		for (i = 0; i < 4 && surf->styles[i] != 0xff; i++) continue;

		lmap->lightmap_luxels += smax * tmax * i;

		//FIXME
		surf->lightpic = (subpic_t *)R_ScrapAlloc (&lmap->rscrap, smax, tmax);
		if (!surf->lightpic) {
			R_ScrapAddLayer (&lmap->rscrap);
			surf->lightpic = (subpic_t *)R_ScrapAlloc (&lmap->rscrap,
														smax, tmax);
			if (!surf->lightpic) {
				Sys_Error ("FIXME taniwha is being lazy");
			}
		}
	}
}

static void
vulkan_create_surfs (model_t *m, lmapctx_t *lmap)
{
	auto brush = m->brush;
	lmap->brush = brush;
	// vis_clusters does not include the solid cluster 0
	uint32_t num_clusters = brush->cluster_vis.count + 1;
	for (uint32_t i = 0; i < num_clusters; i++) {
		auto cluster = &brush->clusters[i];
		vulkan_create_surf_lightmap (cluster, lmap);
	}
	lmap->num_clusters += num_clusters;
	for (uint32_t i = 1; i < brush->numsubmodels; i++) {
		auto cluster = &brush->clusters[num_clusters + i - 1];
		vulkan_create_surf_lightmap (cluster, lmap);
		lmap->num_clusters++;
	}
}

static void
vulkan_init_lightmap (uint32_t surfind, lmapctx_t *lmap)
{
	auto brush = lmap->brush;
	auto lightinfo = lmap->lightinfo;
	auto lightsize = lmap->lightsize;
	auto surfinfo = lmap->surfinfo;
	auto lightmap_data = lmap->lightmap_data;
	auto surf = brush->surfaces + surfind;

	uint ind = lmap->num_lightmaps++;
	if (surf->lightpic) {
		auto r = *(scrapbox_t *) surf->lightpic;//FIXME
		lightinfo[ind] = (bsp_lightinfo_t) {
			.pos = { r.x, r.y, r.layer },
			.width = r.width,
			.styles = { VEC4_EXP (surf->styles) },
			.data = lmap->lightmap_luxels * 3,
		};
		uint32_t samples = r.width * r.height;
		lightsize[ind] = (bsp_lightsize_t) {
			.samples = samples,
			.styles = { VEC4_EXP (surf->styles) },
		};
		surfinfo[ind] = (bsp_surfinfo_t) {
			//FIXME
		};

		if (surf->samples) {
			byte *dst = lightmap_data + lightinfo[ind].data;
			byte *src = surf->samples;
			for (int j = 0; j < 4 && surf->styles[j] != 0xff; j++) {
				memcpy (dst, src, samples * 3);
				dst += samples * 3;
				src += samples * 3;
				lmap->lightmap_luxels += samples;
			}
		}
	} else {
		lightinfo[ind] = (bsp_lightinfo_t) {
			.styles = { VEC4_EXP (surf->styles) },
			.data = ~0,
		};
		lightsize[ind] = (bsp_lightsize_t) {
			.styles = { VEC4_EXP (surf->styles) },
		};
		surfinfo[ind] = (bsp_surfinfo_t) {
			.plane = ~0,
			.tex = ~0,
		};
	}
}

static void
vulkan_build_light_cluster (cluster_t *cluster, lmapctx_t *lmap)
{
	auto brush = lmap->brush;
	for (uint32_t j = 0; j < cluster->count; j++) {
		uint32_t surfind = brush->cluster_surfs[cluster->first + j];
		vulkan_init_lightmap (surfind, lmap);
	}
}

static void
vulkan_build_lightmaps (model_t *m, lmapctx_t *lmap)
{
	auto brush = m->brush;
	lmap->brush = brush;

	// vis_clusters does not include the solid cluster 0
	uint32_t num_clusters = brush->cluster_vis.count + 1;
	for (uint32_t i = 0; i < num_clusters; i++) {
		auto cluster = &brush->clusters[i];
		uint32_t first = lmap->num_lightmaps;
		vulkan_build_light_cluster (cluster, lmap);
		lmap->light_clusters[lmap->num_clusters + i] = (cluster_t) {
			.first = first,
			.count = cluster->count,
		};
	}
	lmap->num_clusters += num_clusters;
	for (uint32_t i = 1; i < brush->numsubmodels; i++) {
		auto cluster = &brush->clusters[num_clusters + i - 1];
		uint32_t first = lmap->num_lightmaps;
		vulkan_build_light_cluster (cluster, lmap);
		lmap->light_clusters[lmap->num_clusters] = (cluster_t) {
			.first = first,
			.count = cluster->count,
		};
		lmap->num_clusters++;
	}
}

static bool
bounds_check_cluster (const cluster_t *cluster, uint32_t max)
{
	if (cluster->first >= max || cluster->first + cluster->count > max) {
		return false;
	}
	return true;
}

/*
  GL_BuildLightmaps

  Builds the lightmap texture with all the surfaces from all brush models
*/
void
Vulkan_BuildLightmaps (model_t **models, int num_models, vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	auto device = ctx->device;
	bspctx_t   *bctx = ctx->bsp_context;
	uint32_t    frames = ctx->render_context->frames.size;

	r_framecount = 1;					// no dlightcache
	lmapctx_t lmap = {
		.ctx = ctx,
	};
	R_ScrapInit (&lmap.rscrap, 512, 512);

	lmap_model_loop (models, num_models, vulkan_create_surfs, &lmap);

	if (bctx->light_scrap) {
		QFV_DestroyScrap (bctx->light_scrap);
		bctx->light_scrap = nullptr;
	}
	bctx->light_scrap = QFV_CreateScrapFromScrap (device, "lightmap_atlas",
												  &lmap.rscrap, tex_frgba,
												  ctx->staging);
	for (size_t i = 0; i < bctx->frames.size; i++) {
		auto frame = &bctx->frames.a[i];
		frame->need_update = true;
	}

	QFV_DestroyResource (ctx->device, bctx->lightmap_resource);
	if (!bctx->lightmap_resource) {
		size_t size = sizeof (qfv_resource_t)
					+ sizeof (qfv_resobj_t)		// lightinfo
					+ sizeof (qfv_resobj_t)		// lightsize
					+ sizeof (qfv_resobj_t)		// surfinfo
					+ sizeof (qfv_resobj_t)		// style data
					+ sizeof (qfv_resobj_t)		// light cache
					+ sizeof (qfv_resobj_t)		// lightmap data
					+ sizeof (qfv_resobj_t)		// light queue offs
					+ sizeof (qfv_resobj_t)		// light queue inds
					+ sizeof (qfv_resobj_t)		// light queue tmp
					+ sizeof (qfv_resobj_t)		// light clusters
					+ sizeof (qfv_resobj_t) 	// light queue
					+ sizeof (qfv_resobj_t)		// light cluster surfs
					+ sizeof (qfv_resobj_t);	// light cluster tmp
		bctx->lightmap_resource = malloc (size);
		*bctx->lightmap_resource = (qfv_resource_t) {
			.name = "bsp:lightmap",
			.va_ctx = ctx->va_ctx,
			.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.num_objects = 13,
			.objects = (qfv_resobj_t *)&bctx->lightmap_resource[1],
		};
	};
	auto lightinfo = &bctx->lightmap_resource->objects[0];
	auto lightsize = &lightinfo[1];
	auto surfinfo = &lightsize[1];
	auto light_cache = &surfinfo[1];
	auto light_style_values = &light_cache[1];
	auto lightmap_data = &light_style_values[1];
	auto light_queue_offs = &lightmap_data[1];
	auto light_queue_inds = &light_queue_offs[1];
	auto light_queue_tmp = &light_queue_inds[1];
	auto light_clusters = &light_queue_tmp[1];
	auto light_queue = &light_clusters[1];
	auto light_cluster_surfs = &light_queue[1];
	auto light_cluster_tmp = &light_cluster_surfs[1];

	*lightinfo = (qfv_resobj_t) {
		.name = "lightinfo",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (bsp_lightinfo_t[lmap.num_lightmaps]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*lightsize = (qfv_resobj_t) {
		.name = "lightsize",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (bsp_lightsize_t[lmap.num_lightmaps]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*surfinfo = (qfv_resobj_t) {
		.name = "surfinfo",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (bsp_surfinfo_t[lmap.num_lightmaps]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*light_cache = (qfv_resobj_t) {
		.name = "light_cache",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (bsp_lightcache_t[lmap.num_lightmaps]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*light_style_values = (qfv_resobj_t) {
		.name = "light_style_values",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (d_lightstylevalue) * frames,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*lightmap_data = (qfv_resobj_t) {
		.name = "lightmap_data",
		.type = qfv_res_buffer,
		.buffer = {
			// always loaded as rgb
			.size = lmap.lightmap_luxels * 3,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*light_queue_offs = (qfv_resobj_t) {
		.name = "light_queue_offs",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (uint32_t[lmap.num_lightmaps]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*light_queue_inds = (qfv_resobj_t) {
		.name = "light_queue_inds",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (uint32_t[lmap.num_lightmaps]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*light_queue_tmp = (qfv_resobj_t) {
		.name = "light_queue_tmp",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (uint32_t[lmap.num_lightmaps]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*light_clusters = (qfv_resobj_t) {
		.name = "light_clusters",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (cluster_t[lmap.num_clusters]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*light_queue = (qfv_resobj_t) {
		.name = "light_queue",
		.type = qfv_res_buffer,
		.buffer = {
			// 4 for count and indirect dispatch, four blocks
			// 2 for prefix sums,
			// 1 for actual lightmap update
			// 1 for actual surface update
			.size = 4 * sizeof (uint32_t[4]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*light_cluster_surfs = (qfv_resobj_t) {
		.name = "light_cluster_surfs",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (cluster_t[lmap.num_clusters]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	*light_cluster_tmp = (qfv_resobj_t) {
		.name = "light_cluster_tmp",
		.type = qfv_res_buffer,
		.buffer = {
			.size = sizeof (cluster_t[lmap.num_clusters]),
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		},
	};
	QFV_CreateResource (ctx->device, bctx->lightmap_resource);

#define BUFFER(b) \
		do { \
			bctx->b##_buffer.buffer = b->buffer.buffer; \
			bctx->b##_buffer.size = b->buffer.size; \
			bctx->b##_buffer.addr = b->buffer.address; \
			if (bctx->b##_buffer.bb) { \
				*bctx->b##_buffer.bb = bctx->b##_buffer.addr; \
			} \
		} while (0)
	BUFFER (lightinfo);
	BUFFER (lightsize);
	BUFFER (surfinfo);
	BUFFER (light_cache);
	BUFFER (light_style_values);
	BUFFER (lightmap_data);
	BUFFER (light_queue_offs);
	BUFFER (light_queue_inds);
	BUFFER (light_queue_tmp);
	BUFFER (light_clusters);
	BUFFER (light_cluster_surfs);
	BUFFER (light_cluster_tmp);
	BUFFER (light_queue);
#undef BUFFER

	bctx->light_surfs = (bsp_prefixsum_t) {
		.in = bctx->light_cluster_surfs_buffer.addr,
		.out = bctx->light_cluster_surfs_buffer.addr,
		.tmp = bctx->light_cluster_tmp_buffer.addr,
		.total = bctx->light_queue_buffer.addr + sizeof (uint32_t[12]),
		.invoke = bctx->light_queue_buffer.buffer,
	};
	bctx->light_luxels = (bsp_prefixsum_t) {
		.in = bctx->light_queue_offs_buffer.addr,
		.out = bctx->light_queue_offs_buffer.addr,
		.tmp = bctx->light_queue_tmp_buffer.addr,
		.total = bctx->light_queue_buffer.addr + sizeof (uint32_t[8]),
		.invoke = bctx->light_queue_buffer.buffer,
	};

	size_t size = lightinfo->buffer.size
				+ lightsize->buffer.size
				+ surfinfo->buffer.size
				+ light_clusters->buffer.size
				+ light_cache->buffer.size
				+ light_style_values->buffer.size
				+ lightmap_data->buffer.size;
	auto packet = QFV_PacketAcquire (ctx->staging, "bsp.lightmap");
	lmap.lightinfo = QFV_PacketExtend (packet, size);
	lmap.lightsize = (bsp_lightsize_t *) &lmap.lightinfo[lmap.num_lightmaps];
	lmap.surfinfo = (bsp_surfinfo_t *) &lmap.lightsize[lmap.num_lightmaps];
	lmap.light_clusters = (cluster_t*) &lmap.surfinfo[lmap.num_lightmaps];
	lmap.light_cache
		= (bsp_lightcache_t *) &lmap.light_clusters[lmap.num_clusters];
	lmap.light_style_values
		= (int16_t *) &lmap.light_clusters[lmap.num_lightmaps];
	lmap.lightmap_data
		= (byte*) &lmap.light_style_values[frames * countof(d_lightstylevalue)];

	for (uint32_t i = 0; i < size / 4; i++) {
		((uint32_t *)lmap.lightinfo)[i] = 0xdeadbeef;
	}
	printf ("lightmap pixels: %d, %d lightmaps, average: %g clusters: %d\n",
			lmap.lightmap_luxels, lmap.num_lightmaps,
			(double) lmap.lightmap_luxels / lmap.num_lightmaps,
			lmap.num_clusters);

	uint32_t num_lightmaps = lmap.num_lightmaps;
	lmap.num_lightmaps = 0;
	lmap.lightmap_luxels = 0;
	lmap.num_clusters = 0;

	lmap_model_loop (models, num_models, vulkan_build_lightmaps, &lmap);

	if (bctx->num_lightmaps) {
		*bctx->num_lightmaps = num_lightmaps;
	}

	for (uint32_t i = 0; i < frames; i++) {
		uint32_t offset = i * sizeof (d_lightstylevalue);
		memcpy ((byte *) lmap.light_style_values + offset, d_lightstylevalue,
				sizeof (d_lightstylevalue));
		bctx->frames.a[i].style_offset = offset;
	}
	memset (lmap.light_cache, 0xff, light_cache->buffer.size);

	bool bad_lightmap = num_lightmaps != lmap.num_lightmaps;
	for (uint32_t i = 0; i < lmap.num_lightmaps; i++) {
		if (lmap.lightinfo[i].data == 0xdeadbeef) {
			printf ("bad lightmap: %d\n", i);
			bad_lightmap = true;
		}
		if (lmap.surfinfo[i].plane == 0xdeadbeef) {
			printf ("bad surfinfo: %d\n", i);
			bad_lightmap = true;
		}
	}
	for (uint32_t i = 0; i < lmap.num_clusters; i++) {
		if (!bounds_check_cluster (&lmap.light_clusters[i],
								   lmap.num_lightmaps)) {
			printf ("what the what?!?: %d %d %d / %d\n", i,
					lmap.light_clusters[i].first,
					lmap.light_clusters[i].count,
					lmap.num_clusters);
			bad_lightmap = true;
		}
	}
	if (bad_lightmap) {
		Sys_Error ("lightmap data incorrect");
	}

#define PACKET_SCATTER(n) \
	do { \
		qfv_scatter_t n##_scatter = { \
			.srcOffset = QFV_PacketOffset (packet, lmap.n), \
			.length = bctx->n##_buffer.size, \
		}; \
		QFV_PacketScatterBuffer (packet, bctx->n##_buffer.buffer, \
				1, &n##_scatter, \
				&bufferBarriers[qfv_BB_Unknown_to_TransferWrite], \
				&bufferBarriers[qfv_BB_TransferWrite_to_ShaderRO]); \
	} while (0)
	PACKET_SCATTER (lightinfo);
	PACKET_SCATTER (lightsize);
	PACKET_SCATTER (surfinfo);
	PACKET_SCATTER (light_cache);
	PACKET_SCATTER (light_style_values);
	PACKET_SCATTER (lightmap_data);
	PACKET_SCATTER (light_clusters);
#undef PACKET_SCATTER
	QFV_PacketSubmit (packet);
}

VkImageView
Vulkan_LightmapImageView (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	bspctx_t   *bctx = ctx->bsp_context;
	return QFV_ScrapImageView (bctx->light_scrap);
}

static void
lightmap_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "lightmap startup");
	auto bctx = ctx->bsp_context;
	auto tctx = ctx->texture_context;

	bctx->dsmanager = QFV_Render_DSManager (ctx, "lightmap_set");
	for (size_t i = 0; i < bctx->frames.size; i++) {
		auto frame = &bctx->frames.a[i];
		frame->lightmap_image = QFV_DSManager_AllocSet (bctx->dsmanager);
		frame->lightmap_descriptor = QFV_DSManager_AllocSet (tctx->dsmanager);
	}

	qfvPopDebug (ctx);
}

void
Vulkan_Lightmap_Init (vulkan_ctx_t *ctx)
{
	QFV_Render_AddStartup (ctx, lightmap_startup);
}
