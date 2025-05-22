/*
	vulkan_apinter.c

	2D drawing based on Guerilla's UIPainter

	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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

#include "QF/cmem.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "compat.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/resource.h"
#include "QF/Vulkan/render.h"
#include "QF/Vulkan/qf_painter.h"

#include "r_internal.h"
#include "vid_vulkan.h"

typedef struct uip_cmd_s {
	uint32_t    cmd;
	uint32_t    next;
} uip_cmd_t;

typedef struct uip_line_s {
	uip_cmd_t   cmd;
	float       a[2];
	float       b[2];
	float       r;
	byte        col[4];
} uip_line_t;

typedef struct uip_circle_s {
	uip_cmd_t   cmd;
	float       c[2];
	float       r;
	byte        col[4];
} uip_circle_t;

typedef struct painterframe_s {
} painterframe_t;

typedef struct painterframeset_s
	DARRAY_TYPE (painterframe_t) painterframeset_t;

typedef struct uip_queue_s
	DARRAY_TYPE (uint32_t) uip_queue_t;

typedef struct painterctx_s {
	qfv_resource_t *resource;
	painterframeset_t frames;
	uint32_t    cmd_width;
	uint32_t    cmd_height;
	uint32_t    max_queues;
	uint32_t   *cmd_heads;
	uint32_t   *cmd_tails;
	uip_queue_t cmd_queue;
} painterctx_t;

static void
painter_shutdown (exprctx_t *ectx)
{
}

static void
painter_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto pctx = ctx->painter_context;
	qfvPushDebug (ctx, "painter startup");

	//auto device = ctx->device;
	//auto dfunc = device->funcs;

	auto rctx = ctx->render_context;
	size_t      frames = rctx->frames.size;
	DARRAY_INIT (&pctx->frames, frames);
	DARRAY_RESIZE (&pctx->frames, frames);
	pctx->frames.grow = 0;
	memset (pctx->frames.a, 0, pctx->frames.size * sizeof (painterframe_t));

	qfvPopDebug (ctx);
}

static void
painter_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	QFV_Render_AddShutdown (ctx, painter_shutdown);
	QFV_Render_AddStartup (ctx, painter_startup);

	painterctx_t *pctx = malloc (sizeof (painterctx_t));
	ctx->painter_context = pctx;

	*pctx = (painterctx_t) {
		.cmd_width = 240,//FIXME
		.cmd_height = 135,//FIXME
		.max_queues = 32400,//FIXME
		.cmd_queue = DARRAY_STATIC_INIT (1024),
	};

	pctx->cmd_heads = malloc (pctx->max_queues * 2 * sizeof (uint32_t));
	pctx->cmd_tails = &pctx->cmd_heads[pctx->max_queues];
	memset (pctx->cmd_heads, 0xff, pctx->max_queues * 2 * sizeof (uint32_t));
}

static void
painter_draw (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
}

static exprfunc_t painter_draw_func[] = {
	{ .func = painter_draw },
	{}
};
static exprfunc_t painter_init_func[] = {
	{ .func = painter_init },
	{}
};

static exprsym_t painter_task_syms[] = {
	{ "painter_draw", &cexpr_function, painter_draw_func },
	{ "painter_init", &cexpr_function, painter_init_func },
	{}
};

void
Vulkan_Painter_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, painter_task_syms);
}

static void
painter_queue_cmd (int x, int y, uip_cmd_t *cmd, uint32_t size,
				   painterctx_t *pctx)
{
	uint32_t    queue_ind = y * pctx->cmd_width + x;
	auto head_ptr = &pctx->cmd_heads[queue_ind];
	auto tail_ptr = &pctx->cmd_tails[queue_ind];
	uint32_t    tail = *tail_ptr;
	uint32_t    ind = pctx->cmd_queue.size;
	uint32_t    count = size / sizeof (uint32_t);
	auto queued_cmd = (uip_cmd_t *) DARRAY_OPEN_AT (&pctx->cmd_queue, ind,
													count);
	if (*head_ptr == ~0u) {
		*head_ptr = ind;
	}
	memcpy (queued_cmd, cmd, size);
	if (tail != ~0u) {
		pctx->cmd_queue.a[tail] = ind;
	}
	*tail_ptr = &queued_cmd->next - pctx->cmd_queue.a;
}

static void
painter_add_command (vec2f_t tl, vec2f_t br, uip_cmd_t *cmd, uint32_t size,
					 painterctx_t *pctx)
{
	int x1 = tl[0] / 8;
	int y1 = tl[1] / 8;
	int x2 = br[0] / 8 + 1;
	int y2 = br[1] / 8 + 1;

	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;
	if (x2 > (int) pctx->cmd_width) x2 = pctx->cmd_width;
	if (y2 > (int) pctx->cmd_height) y2 = pctx->cmd_height;
	for (int y = y1; y < y2; y++) {
		for (int x = x1; x < x2; x++) {
			painter_queue_cmd (x, y, cmd, size, pctx);
		}
	}
}

void
Vulkan_Painter_AddLine (vec2f_t p1, vec2f_t p2, float r, const quat_t color,
						vulkan_ctx_t *ctx)
{
	auto pctx = ctx->painter_context;
	uip_line_t line = {
		.cmd = {
			.cmd = 0,
			.next = ~0u,
		},
		.a = { VEC2_EXP (p1) },
		.b = { VEC2_EXP (p2) },
		.r = r,
	};
	QuatScale (color, 255, line.col);
	if (p1[0] > p2[0]) {
		float t = p1[0];
		p1[0] = p2[0];
		p2[0] = t;
	}
	if (p1[1] > p2[1]) {
		float t = p1[1];
		p1[1] = p2[1];
		p2[1] = t;
	}
	//FIXME use modified xiaolin wu's line algorithm
	painter_add_command (p1 - r, p2 + r, &line.cmd, sizeof (line), pctx);
}

void
Vulkan_Painter_AddCircle (vec2f_t c, float r, const quat_t color,
						  vulkan_ctx_t *ctx)
{
	auto pctx = ctx->painter_context;
	uip_circle_t circle = {
		.cmd = {
			.cmd = 1,
			.next = ~0u,
		},
		.c = { VEC2_EXP (c) },
		.r = r,
	};
	QuatScale (color, 255, circle.col);
	//FIXME use filled circle?
	painter_add_command (c - r, c + r, &circle.cmd, sizeof (circle), pctx);
}
