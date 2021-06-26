/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include "QF/cvar.h"
#include "QF/qtypes.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "QF/plugin/general.h"

#include "r_internal.h"
#include "r_scrap.h"
#include "vid_internal.h"

cvar_t         *vidrend_plugin;
plugin_t       *vidrendmodule = NULL;

VID_RENDER_PLUGIN_PROTOS
static plugin_list_t vidrend_plugin_list[] = {
	    VID_RENDER_PLUGIN_LIST
};

vid_render_data_t *r_data;
vid_render_funcs_t *r_funcs;

#define U __attribute__ ((used))
static U void (*const r_progs_init)(struct progs_s *) = R_Progs_Init;
static U void (*const r_scrapdelete)(rscrap_t *) = R_ScrapDelete;
#undef U

static void
R_shutdown (void *data)
{
	if (vidrendmodule->functions->general->shutdown) {
		vidrendmodule->functions->general->shutdown ();
	}
}

VISIBLE void
R_LoadModule (vid_internal_t *vid_internal)
{
	PI_RegisterPlugins (vidrend_plugin_list);
	vidrend_plugin = Cvar_Get ("vid_render", VID_RENDER_DEFAULT, CVAR_ROM, 0,
							   "Video Render Plugin to use");
	vidrendmodule = PI_LoadPlugin ("vid_render", vidrend_plugin->string);
	if (!vidrendmodule) {
		Sys_Error ("Loading of video render module: %s failed!\n",
				   vidrend_plugin->string);
	}
	r_funcs = vidrendmodule->functions->vid_render;
	mod_funcs = r_funcs->model_funcs;
	r_data = vidrendmodule->data->vid_render;
	r_data->vid->vid_internal = vid_internal;

	r_funcs->init ();
	Sys_RegisterShutdown (R_shutdown, 0);
}

VISIBLE void
R_Init (void)
{
	r_funcs->R_Init ();
}
