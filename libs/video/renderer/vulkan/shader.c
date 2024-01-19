/*
	shader.c

	Vulkan shader manager

	Copyright (C) 2020      Bill Currie <bill@taniwha.org>

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

#include <string.h>

#include "QF/dstring.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "QF/Vulkan/debug.h"
#include "QF/Vulkan/device.h"
#include "QF/Vulkan/shader.h"

static
#include "libs/video/renderer/vulkan/shader/slice.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/line.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/line.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/particle.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/particle.geom.spvc"
static
#include "libs/video/renderer/vulkan/shader/particle.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/partphysics.comp.spvc"
static
#include "libs/video/renderer/vulkan/shader/partupdate.comp.spvc"
static
#include "libs/video/renderer/vulkan/shader/sprite.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/sprite_gbuf.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/sprite_gbuf.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/sprite_depth.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/sprite_depth.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/twod_depth.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/twod.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/twod.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/quakebsp.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/quakebsp.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/bsp_depth.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/bsp_gbuf.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/bsp_gbuf.geom.spvc"
static
#include "libs/video/renderer/vulkan/shader/bsp_gbuf.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/bsp_shadow.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/bsp_sky.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/bsp_turb.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/debug.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/entid.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/light_entid.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/light_flat.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/light_splat.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/light_splat.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/light_debug.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/light_oit.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/lighting_cascade.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/lighting_cube.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/lighting_none.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/lighting_plane.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/compose.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/compose_fwd.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/alias.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/alias_depth.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/alias.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/alias_gbuf.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/alias_shadow.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/iqm.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/iqm.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/iqm_fwd.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/output.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/passthrough.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/fstriangle.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/fstrianglest.vert.spvc"
static
#include "libs/video/renderer/vulkan/shader/gridplane.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/pushcolor.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/fisheye.frag.spvc"
static
#include "libs/video/renderer/vulkan/shader/waterwarp.frag.spvc"

typedef struct shaderdata_s {
	const char *name;
	const uint32_t *data;
	size_t      size;
} shaderdata_t;

static shaderdata_t builtin_shaders[] = {
	{ "slice.vert", slice_vert, sizeof (slice_vert) },
	{ "line.vert", line_vert, sizeof (line_vert) },
	{ "line.frag", line_frag, sizeof (line_frag) },
	{ "particle.vert", particle_vert, sizeof (particle_vert) },
	{ "particle.geom", particle_geom, sizeof (particle_geom) },
	{ "particle.frag", particle_frag, sizeof (particle_frag) },
	{ "partphysics.comp", partphysics_comp, sizeof (partphysics_comp) },
	{ "partupdate.comp", partupdate_comp, sizeof (partupdate_comp) },
	{ "sprite.frag", sprite_frag, sizeof (sprite_frag) },
	{ "sprite_gbuf.vert", sprite_gbuf_vert, sizeof (sprite_gbuf_vert) },
	{ "sprite_gbuf.frag", sprite_gbuf_frag, sizeof (sprite_gbuf_frag) },
	{ "sprite_depth.vert", sprite_depth_vert, sizeof (sprite_depth_vert) },
	{ "sprite_depth.frag", sprite_depth_frag, sizeof (sprite_depth_frag) },
	{ "twod_depth.frag", twod_depth_frag, sizeof (twod_depth_frag) },
	{ "twod.vert", twod_vert, sizeof (twod_vert) },
	{ "twod.frag", twod_frag, sizeof (twod_frag) },
	{ "quakebsp.vert", quakebsp_vert, sizeof (quakebsp_vert) },
	{ "quakebsp.frag", quakebsp_frag, sizeof (quakebsp_frag) },
	{ "bsp_depth.vert", bsp_depth_vert, sizeof (bsp_depth_vert) },
	{ "bsp_gbuf.vert", bsp_gbuf_vert, sizeof (bsp_gbuf_vert) },
	{ "bsp_gbuf.geom", bsp_gbuf_geom, sizeof (bsp_gbuf_geom) },
	{ "bsp_gbuf.frag", bsp_gbuf_frag, sizeof (bsp_gbuf_frag) },
	{ "bsp_shadow.vert", bsp_shadow_vert, sizeof (bsp_shadow_vert) },
	{ "bsp_sky.frag", bsp_sky_frag, sizeof (bsp_sky_frag) },
	{ "bsp_turb.frag", bsp_turb_frag, sizeof (bsp_turb_frag) },
	{ "debug.frag", debug_frag, sizeof (debug_frag) },
	{ "entid.frag", entid_frag, sizeof (entid_frag) },
	{ "light_entid.vert", light_entid_vert, sizeof (light_entid_vert) },
	{ "light_flat.vert", light_flat_vert, sizeof (light_flat_vert) },
	{ "light_splat.vert", light_splat_vert, sizeof (light_splat_vert) },
	{ "light_splat.frag", light_splat_frag, sizeof (light_splat_frag) },
	{ "light_debug.frag", light_debug_frag, sizeof (light_debug_frag) },
	{ "light_oit.frag", light_oit_frag, sizeof (light_oit_frag) },
	{ "lighting_cascade.frag", lighting_cascade_frag,
		sizeof (lighting_cascade_frag) },
	{ "lighting_cube.frag", lighting_cube_frag, sizeof (lighting_cube_frag) },
	{ "lighting_none.frag", lighting_none_frag, sizeof (lighting_none_frag) },
	{ "lighting_plane.frag", lighting_plane_frag,
		sizeof (lighting_plane_frag) },
	{ "compose.frag", compose_frag, sizeof (compose_frag) },
	{ "compose_fwd.frag", compose_fwd_frag, sizeof (compose_fwd_frag) },
	{ "alias.vert", alias_vert, sizeof (alias_vert) },
	{ "alias_depth.vert", alias_depth_vert, sizeof (alias_depth_vert) },
	{ "alias.frag", alias_frag, sizeof (alias_frag) },
	{ "alias_gbuf.frag", alias_gbuf_frag, sizeof (alias_gbuf_frag) },
	{ "alias_shadow.vert", alias_shadow_vert, sizeof (alias_shadow_vert) },
	{ "iqm.vert", iqm_vert, sizeof (iqm_vert) },
	{ "iqm.frag", iqm_frag, sizeof (iqm_frag) },
	{ "iqm_fwd.frag", iqm_fwd_frag, sizeof (iqm_fwd_frag) },
	{ "output.frag", output_frag, sizeof (output_frag) },
	{ "passthrough.vert", passthrough_vert, sizeof (passthrough_vert) },
	{ "fstriangle.vert", fstriangle_vert, sizeof (fstriangle_vert) },
	{ "fstrianglest.vert", fstrianglest_vert, sizeof (fstrianglest_vert) },
	{ "gridplane.frag", gridplane_frag, sizeof (gridplane_frag) },
	{ "pushcolor.frag", pushcolor_frag, sizeof (pushcolor_frag) },
	{ "fisheye.frag", fisheye_frag, sizeof (fisheye_frag) },
	{ "waterwarp.frag", waterwarp_frag, sizeof (waterwarp_frag) },
	{}
};

#define BUILTIN "$builtin/"
#define BUILTIN_SIZE (sizeof (BUILTIN) - 1)
#define SHADER "$shader/"
#define SHADER_SIZE (sizeof (SHADER) - 1)

VkShaderModule
QFV_CreateShaderModule (qfv_device_t *device, const char *shader_path)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	shaderdata_t _data = {};
	shaderdata_t *data = 0;
	dstring_t  *path = 0;
	QFile      *file = 0;
	VkShaderModule shader = 0;

	if (strncmp (shader_path, BUILTIN, BUILTIN_SIZE) == 0) {
		const char *name = shader_path + BUILTIN_SIZE;

		for (int i = 0; builtin_shaders[i].name; i++) {
			if (strcmp (builtin_shaders[i].name, name) == 0) {
				data = &builtin_shaders[i];
				break;
			}
		}
	} else if (strncmp (shader_path, SHADER, SHADER_SIZE)) {
		path = dstring_new ();
		dsprintf (path, "%s/%s", FS_SHADERPATH, shader_path + SHADER_SIZE);
		file = Qopen (path->str, "rbz");
	} else {
		file = QFS_FOpenFile (shader_path);
	}

	if (file) {
		_data.size = Qfilesize (file);
		_data.data = malloc (_data.size);
		Qread (file, (void *) _data.data, _data.size);
		Qclose (file);
		data = &_data;
	}

	if (data) {
		Sys_MaskPrintf (SYS_vulkan,
						"QFV_CreateShaderModule: creating shader module %s\n",
						shader_path);
		VkShaderModuleCreateInfo createInfo = {
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, 0,
			0, data->size, data->data
		};

		dfunc->vkCreateShaderModule (dev, &createInfo, 0, &shader);
		QFV_duSetObjectName (device, VK_OBJECT_TYPE_SHADER_MODULE, shader,
							 shader_path);
	} else {
		Sys_MaskPrintf (SYS_vulkan,
						"QFV_CreateShaderModule: could not find shader %s\n",
						shader_path);
	}

	if (path) {
		dstring_delete (path);
	}
	if (_data.data) {
		free ((void *) _data.data);
	}
	return shader;
}

void
QFV_DestroyShaderModule (qfv_device_t *device, VkShaderModule module)
{
	VkDevice    dev = device->dev;
	qfv_devfuncs_t *dfunc = device->funcs;

	dfunc->vkDestroyShaderModule (dev, module, 0);
}
