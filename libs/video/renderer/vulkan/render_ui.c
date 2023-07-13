/*
	render_ui.c

	Vulkan render manager UI

	Copyright (C) 2023      Bill Currie <bill@taniwha.org>

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

#include "QF/va.h"
#include "QF/ui/imui.h"

#include "QF/Vulkan/render.h"
#include "vid_vulkan.h"

#define IMUI_context imui_ctx

static void
job_window (vulkan_ctx_t *ctx, imui_ctx_t *imui_ctx)
{
	auto rctx = ctx->render_context;
	if (!rctx->job_window) {
		rctx->job_window = malloc (sizeof (imui_window_t));
		*rctx->job_window = (imui_window_t) {
			.name = nva ("Job##%p.window", rctx),
			.xpos = 100,
			.ypos = 50,
		};
	}
	UI_Window (rctx->job_window) {
		if (rctx->job_window->is_collapsed) {
			continue;
		}
		auto job = rctx->job;
		for (uint32_t i = 0; i < job->num_steps; i++) {
			auto step = &job->steps[i];
			UI_Horizontal {
				UI_Labelf ("%s##%p.job.step.%d", step->label.name, rctx, i);
				UI_FlexibleSpace ();
			}
		}
	}
}

void
QFV_Render_UI (vulkan_ctx_t *ctx, imui_ctx_t *imui_ctx)
{
	auto rctx = ctx->render_context;
	if (UI_Button (va (ctx->va_ctx, "Job##%p", rctx))) {
		rctx->job_window->is_open = true;
	}
	job_window (ctx, imui_ctx);
}

void
QFV_Render_UI_Shutdown (vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	if (rctx->job_window) {
		free ((char *) rctx->job_window->name);
		free (rctx->job_window);
	}
}
