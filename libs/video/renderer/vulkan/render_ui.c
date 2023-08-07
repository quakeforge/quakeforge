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

#include "QF/fbsearch.h"
#include "QF/va.h"
#include "QF/ui/imui.h"

#include "QF/Vulkan/mouse_pick.h"
#include "QF/Vulkan/render.h"
#include "vid_vulkan.h"

#define IMUI_context imui_ctx

#define picked_entity_count (mousepick_size * mousepick_size)

typedef struct qfv_renderdebug_s {
	imui_window_t job_timings_window;
	imui_window_t job_control_window;
	imui_window_t entity_window;
	uint32_t picked_enties[picked_entity_count];
} qfv_renderdebug_t;

static void
hs (imui_ctx_t *imui_ctx, int size)
{
	IMUI_Spacer (imui_ctx, imui_size_pixels, 30 * size, imui_size_expand, 100);
}

static void
reset_time (qfv_time_t *time)
{
	time->min_time = INT64_MAX;
	time->max_time = -INT64_MAX;
}

static void
show_time (qfv_time_t *time, imui_ctx_t *imui_ctx, const char *suffix)
{
	UI_Labelf ("%'7"PRIu64"\u03bcs%s", time->min_time, suffix);
	UI_Labelf ("%'7"PRIu64"\u03bcs%s", time->cur_time, suffix);
	UI_Labelf ("%'7"PRIu64"\u03bcs%s", time->max_time, suffix);
}

static void
job_timings_window (vulkan_ctx_t *ctx, imui_ctx_t *imui_ctx)
{
	auto rctx = ctx->render_context;
	UI_Window (&rctx->debug->job_timings_window) {
		if (rctx->debug->job_timings_window.is_collapsed) {
			continue;
		}
		auto job = rctx->job;
		for (uint32_t i = 0; i < job->num_steps; i++) {
			auto step = &job->steps[i];
			UI_Horizontal {
				UI_Labelf ("%s##%p.job.step.%d", step->label.name, rctx, i);
				UI_FlexibleSpace ();
				show_time (&step->time, imui_ctx,
						   va (ctx->va_ctx, "##%p.job.step.%d.time", rctx, i));
			}
		}
		UI_Horizontal {
			UI_FlexibleSpace ();
			show_time (&job->time, imui_ctx,
					   va (ctx->va_ctx, "##%p.job.time", rctx));
		}
		UI_Horizontal {
			UI_FlexibleSpace ();
			if (UI_Button ("Reset##job.timings")) {
				for (uint32_t i = 0; i < job->num_steps; i++) {
					auto step = &job->steps[i];
					reset_time (&step->time);
				}
				reset_time (&job->time);
			}
		}
	}
}

static void
show_pipeline (const char *type, qfv_pipeline_t *pipeline,
			   imui_ctx_t *imui_ctx, vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	int indent;
	if (!strcmp (type, "compute")) {
		indent = 1;
	} else {
		indent = 3;
	}
	UI_Horizontal {
		hs (imui_ctx, indent);
		UI_Labelf ("%s##%p.pipeline.%p.%s",
				   pipeline->label.name, rctx, pipeline, type);
		UI_FlexibleSpace ();
		UI_Checkbox (&pipeline->disabled,
					 va (ctx->va_ctx, "##pipeline.disabled.%p", pipeline));
	}
}

static void
job_control_window (vulkan_ctx_t *ctx, imui_ctx_t *imui_ctx)
{
	auto rctx = ctx->render_context;
	UI_Window (&rctx->debug->job_control_window) {
		if (rctx->debug->job_control_window.is_collapsed) {
			continue;
		}
		auto job = rctx->job;
		for (uint32_t i = 0; i < job->num_steps; i++) {
			auto step = &job->steps[i];
			UI_Horizontal {
				UI_Labelf ("%s##%p.job.step.%d", step->label.name, rctx, i);
				UI_FlexibleSpace ();
			}
			if (step->render) {
				auto rp = step->render->active;
				UI_Horizontal {
					hs (imui_ctx, 1);
					UI_Labelf ("%s##%p.job.step.%d.render", rp->label.name,
							   rctx, i);
					UI_FlexibleSpace ();
				}
				for (uint32_t j = 0; j < rp->subpass_count; j++) {
					auto sp = &rp->subpasses[j];
					UI_Horizontal {
						hs (imui_ctx, 2);
						UI_Labelf ("%s##%p.job.step.%d.subpass",
								   sp->label.name, rctx, i);
						UI_FlexibleSpace ();
					}
					for (uint32_t j = 0; j < sp->pipeline_count; j++) {
						auto pipeline = &sp->pipelines[j];
						show_pipeline ("graphics", pipeline, imui_ctx, ctx);
					}
				}
			}
			if (step->compute) {
				auto compute = step->compute;
				for (uint32_t j = 0; j < compute->pipeline_count; j++) {
					auto pipeline = &compute->pipelines[j];
					show_pipeline ("compute", pipeline, imui_ctx, ctx);
				}
			}
			if (step->process) {
			}
		}
	}
}

static void
mousepick_callback (const uint32_t *entid, void *data)
{
	auto debug = ((qfv_renderctx_t *) data)->debug;
	memcpy (debug->picked_enties, entid, sizeof (debug->picked_enties));
}

static int
entid_counts_cmp (const void *_a, const void *_b)
{
	const uint32_t *a = _a;
	const uint32_t *b = _b;
	return *a - *b;
}

static void
entity_window (vulkan_ctx_t *ctx, imui_ctx_t *imui_ctx)
{
	auto rctx = ctx->render_context;
	auto debug = rctx->debug;
	UI_Window (&debug->entity_window) {
		if (debug->entity_window.is_collapsed) {
			continue;
		}
		typedef struct {
			uint32_t entid;
			uint32_t count;
		} entid_counts_t;
		entid_counts_t entid_counts[picked_entity_count];
		uint32_t num_entids = 0;
		for (uint32_t i = 0; i < picked_entity_count; i++) {
			if (debug->picked_enties[i] == nullent) {
				continue;
			}
			if (!num_entids) {
				entid_counts[num_entids++] = (entid_counts_t) {
					.entid = debug->picked_enties[i],
					.count = 1,
				};
				continue;
			}
			entid_counts_t *ec = 0;
			ec = fbsearch (&debug->picked_enties[i],
						   entid_counts, num_entids, sizeof (entid_counts_t),
						   entid_counts_cmp);
			uint32_t ind = 0;
			if (ec) {
				if (ec->entid == debug->picked_enties[i]) {
					ec->count++;
					continue;
				}
				ind = ec - entid_counts + 1;
			}
			memmove (entid_counts + ind + 1, entid_counts + ind,
					 sizeof (entid_counts_t[num_entids - ind]));
			entid_counts[ind] = (entid_counts_t) {
				.entid = debug->picked_enties[i],
				.count = 1,
			};
			num_entids++;
		}

		auto io = IMUI_GetIO (imui_ctx);
		if (io.hot == nullent && io.active == nullent) {
			QFV_MousePick_Read (ctx, io.mouse.x, io.mouse.y,
								mousepick_callback, rctx);
			for (uint32_t i = num_entids; i-- > 0; ) {
				UI_Horizontal {
					UI_Label ("Entity");
					hs (imui_ctx, 1);
					UI_FlexibleSpace ();
					UI_Labelf ("%08x %2d##%p.entity", entid_counts[i].entid,
							   entid_counts[i].count, rctx);
				}
			}
		}
	}
}

void
QFV_Render_UI (vulkan_ctx_t *ctx, imui_ctx_t *imui_ctx)
{
	auto rctx = ctx->render_context;
	if (!rctx->debug) {
		rctx->debug = malloc (sizeof (qfv_renderdebug_t));
		*rctx->debug = (qfv_renderdebug_t) {
			.job_timings_window = {
				.name = nva ("Job Timings##%p.window", rctx),
				.xpos = 100,
				.ypos = 50,
			},
			.job_control_window = {
				.name = nva ("Job Control##%p.window", rctx),
				.xpos = 100,
				.ypos = 50,
			},
			.entity_window = {
				.name = nva ("Entities##%p.window", rctx),
				.xpos = 100,
				.ypos = 50,
			},
		};
		memset (rctx->debug->picked_enties, 0xff,
				sizeof (rctx->debug->picked_enties));
	}
	job_timings_window (ctx, imui_ctx);
	job_control_window (ctx, imui_ctx);
	entity_window (ctx, imui_ctx);
}

void
QFV_Render_Menu (vulkan_ctx_t *ctx, imui_ctx_t *imui_ctx)
{
	auto rctx = ctx->render_context;
	if (UI_MenuItem (va (ctx->va_ctx, "Job Timings##%p", rctx))) {
		rctx->debug->job_timings_window.is_open = true;
	}
	if (UI_MenuItem (va (ctx->va_ctx, "Job Control##%p", rctx))) {
		rctx->debug->job_control_window.is_open = true;
	}
	if (UI_MenuItem (va (ctx->va_ctx, "Entities##%p", rctx))) {
		rctx->debug->entity_window.is_open = true;
	}
}

void
QFV_Render_UI_Shutdown (vulkan_ctx_t *ctx)
{
	auto rctx = ctx->render_context;
	if (rctx->debug) {
		free ((char *) rctx->debug->job_timings_window.name);
		free ((char *) rctx->debug->job_control_window.name);
		free ((char *) rctx->debug->entity_window.name);
		free (rctx->debug);
	}
}
