/*
	vid_render_gl.c

	Common functions and data for the video plugins

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

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

#include "QF/plugin/general.h"
#include "QF/plugin/vid_render.h"

#include "QF/ui/view.h"

#include "mod_internal.h"
#include "r_internal.h"

viddef_t    vid;					// global video state
view_t      scr_view;

vid_render_data_t vid_render_data = {
	&vid, &r_refdef, &scr_view,
	0, 0, 0,
	0,
	0, 0,
	0.0,
	false, false, false,
	0,
	0, 0,
	0.0, 0.0,
	0,
};

vid_render_funcs_t *vid_render_funcs;
