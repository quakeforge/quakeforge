/*
	vulkan_compose.c

	Vulkan compose pass pipeline

	Copyright (C) 2021       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/2/23

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
#include <stdlib.h>

#include "qfalloca.h"

#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/Vulkan/qf_compose.h"
#include "QF/Vulkan/qf_translucent.h"
#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/dsmanager.h"
#include "QF/Vulkan/image.h"
#include "QF/Vulkan/instance.h"
#include "QF/Vulkan/render.h"

#include "r_internal.h"
#include "vid_vulkan.h"

static void
compose_update (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneNamed (zone, true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	auto cctx = ctx->compose_context;

	if (cctx->camera) {
		*cctx->fog = Fog_Get ();
		*cctx->camera = r_refdef.camera[3];
		*cctx->near_plane = r_nearclip;
	}
}

static void
compose_shutdown (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	composectx_t *cctx = ctx->compose_context;

	free (cctx);
}

static void
compose_startup (exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;
	qfvPushDebug (ctx, "compose startup");

	qfvPopDebug (ctx);
}

static void
compose_init (const exprval_t **params, exprval_t *result, exprctx_t *ectx)
{
	qfZoneScoped (true);
	auto taskctx = (qfv_taskctx_t *) ectx;
	auto ctx = taskctx->ctx;

	QFV_Render_AddShutdown (ctx, compose_shutdown);
	QFV_Render_AddStartup (ctx, compose_startup);

	composectx_t *cctx = malloc (sizeof (composectx_t));
	ctx->compose_context = cctx;
	*cctx = (composectx_t) {
		.fog        = QFV_GetBlackboardVar (ctx, "fog"),
		.camera     = QFV_GetBlackboardVar (ctx, "camera"),
		.near_plane = QFV_GetBlackboardVar (ctx, "near_plane"),
	};
}

static exprtype_t *compose_update_params[] = {
	&cexpr_int,
};
static exprfunc_t compose_update_func[] = {
	{ .func = compose_update, .num_params = 1,
		.param_types = compose_update_params },
	{}
};

static exprfunc_t compose_init_func[] = {
	{ .func = compose_init },
	{}
};

static exprsym_t compose_task_syms[] = {
	{ "compose_update", &cexpr_function, compose_update_func },
	{ "compose_init", &cexpr_function, compose_init_func },
	{}
};

void
Vulkan_Compose_Init (vulkan_ctx_t *ctx)
{
	qfZoneScoped (true);
	QFV_Render_AddTasks (ctx, compose_task_syms);
}
