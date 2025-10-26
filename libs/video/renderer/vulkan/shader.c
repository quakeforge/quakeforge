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

static const byte alignas(uint32_t) slice_vert[] = {
#embed "libs/video/renderer/vulkan/shader/slice.vert.spv"
};
static const byte alignas(uint32_t) line_vert[] = {
#embed "libs/video/renderer/vulkan/shader/line.vert.spv"
};
static const byte alignas(uint32_t) line_frag[] = {
#embed "libs/video/renderer/vulkan/shader/line.frag.spv"
};
static const byte alignas(uint32_t) particle_vert[] = {
#embed "libs/video/renderer/vulkan/shader/particle.vert.spv"
};
static const byte alignas(uint32_t) particle_geom[] = {
#embed "libs/video/renderer/vulkan/shader/particle.geom.spv"
};
static const byte alignas(uint32_t) particle_frag[] = {
#embed "libs/video/renderer/vulkan/shader/particle.frag.spv"
};
static const byte alignas(uint32_t) partphysics_comp[] = {
#embed "libs/video/renderer/vulkan/shader/partphysics.comp.spv"
};
static const byte alignas(uint32_t) partupdate_comp[] = {
#embed "libs/video/renderer/vulkan/shader/partupdate.comp.spv"
};
static const byte alignas(uint32_t) sprite_frag[] = {
#embed "libs/video/renderer/vulkan/shader/sprite.frag.spv"
};
static const byte alignas(uint32_t) sprite_gbuf_vert[] = {
#embed "libs/video/renderer/vulkan/shader/sprite_gbuf.vert.spv"
};
static const byte alignas(uint32_t) sprite_gbuf_frag[] = {
#embed "libs/video/renderer/vulkan/shader/sprite_gbuf.frag.spv"
};
static const byte alignas(uint32_t) sprite_depth_vert[] = {
#embed "libs/video/renderer/vulkan/shader/sprite_depth.vert.spv"
};
static const byte alignas(uint32_t) sprite_depth_frag[] = {
#embed "libs/video/renderer/vulkan/shader/sprite_depth.frag.spv"
};
static const byte alignas(uint32_t) twod_depth_frag[] = {
#embed "libs/video/renderer/vulkan/shader/twod_depth.frag.spv"
};
static const byte alignas(uint32_t) twod_vert[] = {
#embed "libs/video/renderer/vulkan/shader/twod.vert.spv"
};
static const byte alignas(uint32_t) twod_frag[] = {
#embed "libs/video/renderer/vulkan/shader/twod.frag.spv"
};
static const byte alignas(uint32_t) quakebsp_vert[] = {
#embed "libs/video/renderer/vulkan/shader/quakebsp.vert.spv"
};
static const byte alignas(uint32_t) quakebsp_frag[] = {
#embed "libs/video/renderer/vulkan/shader/quakebsp.frag.spv"
};
static const byte alignas(uint32_t) bsp_depth_vert[] = {
#embed "libs/video/renderer/vulkan/shader/bsp_depth.vert.spv"
};
static const byte alignas(uint32_t) bsp_gbuf_vert[] = {
#embed "libs/video/renderer/vulkan/shader/bsp_gbuf.vert.spv"
};
static const byte alignas(uint32_t) bsp_gbuf_frag[] = {
#embed "libs/video/renderer/vulkan/shader/bsp_gbuf.frag.spv"
};
static const byte alignas(uint32_t) bsp_shadow_vert[] = {
#embed "libs/video/renderer/vulkan/shader/bsp_shadow.vert.spv"
};
static const byte alignas(uint32_t) bsp_sky_frag[] = {
#embed "libs/video/renderer/vulkan/shader/bsp_sky.frag.spv"
};
static const byte alignas(uint32_t) bsp_turb_frag[] = {
#embed "libs/video/renderer/vulkan/shader/bsp_turb.frag.spv"
};
static const byte alignas(uint32_t) debug_frag[] = {
#embed "libs/video/renderer/vulkan/shader/debug.frag.spv"
};
static const byte alignas(uint32_t) entid_frag[] = {
#embed "libs/video/renderer/vulkan/shader/entid.frag.spv"
};
static const byte alignas(uint32_t) light_entid_r[] = {
#embed "libs/video/renderer/vulkan/shader/light_entid.r.spv"
};
static const byte alignas(uint32_t) light_splat_r[] = {
#embed "libs/video/renderer/vulkan/shader/light_splat.r.spv"
};
static const byte alignas(uint32_t) light_debug_r[] = {
#embed "libs/video/renderer/vulkan/shader/light_debug.r.spv"
};
static const byte alignas(uint32_t) light_oit_r[] = {
#embed "libs/video/renderer/vulkan/shader/light_oit.r.spv"
};
static const byte alignas(uint32_t) lighting_cascade_r[] = {
#embed "libs/video/renderer/vulkan/shader/lighting_cascade.r.spv"
};
static const byte alignas(uint32_t) lighting_cube_r[] = {
#embed "libs/video/renderer/vulkan/shader/lighting_cube.r.spv"
};
static const byte alignas(uint32_t) lighting_none_r[] = {
#embed "libs/video/renderer/vulkan/shader/lighting_none.r.spv"
};
static const byte alignas(uint32_t) lighting_plane_r[] = {
#embed "libs/video/renderer/vulkan/shader/lighting_plane.r.spv"
};
static const byte alignas(uint32_t) compose_r[] = {
#embed "libs/video/renderer/vulkan/shader/compose.r.spv"
};
static const byte alignas(uint32_t) compose_fwd_r[] = {
#embed "libs/video/renderer/vulkan/shader/compose_fwd.r.spv"
};
static const byte alignas(uint32_t) mesh_r[] = {
#embed "libs/video/renderer/vulkan/shader/mesh.r.spv"
};
static const byte alignas(uint32_t) mesh_shadow_r[] = {
#embed "libs/video/renderer/vulkan/shader/mesh_shadow.r.spv"
};
static const byte alignas(uint32_t) qskin_fwd_frag[] = {
#embed "libs/video/renderer/vulkan/shader/qskin_fwd.frag.spv"
};
static const byte alignas(uint32_t) qskin_gbuf_frag[] = {
#embed "libs/video/renderer/vulkan/shader/qskin_gbuf.frag.spv"
};
static const byte alignas(uint32_t) output_frag[] = {
#embed "libs/video/renderer/vulkan/shader/output.frag.spv"
};
static const byte alignas(uint32_t) painter_r[] = {
#embed "libs/video/renderer/vulkan/shader/painter.r.spv"
};
static const byte alignas(uint32_t) expand_r[] = {
#embed "libs/video/renderer/vulkan/shader/expand.r.spv"
};
static const byte alignas(uint32_t) queueobj_r[] = {
#embed "libs/video/renderer/vulkan/shader/queueobj.r.spv"
};
static const byte alignas(uint32_t) gizmo_r[] = {
#embed "libs/video/renderer/vulkan/shader/gizmo.r.spv"
};
static const byte alignas(uint32_t) passthrough_vert[] = {
#embed "libs/video/renderer/vulkan/shader/passthrough.vert.spv"
};
static const byte alignas(uint32_t) fstriangle_vert[] = {
#embed "libs/video/renderer/vulkan/shader/fstriangle.vert.spv"
};
static const byte alignas(uint32_t) fstrianglest_vert[] = {
#embed "libs/video/renderer/vulkan/shader/fstrianglest.vert.spv"
};
static const byte alignas(uint32_t) gridplane_frag[] = {
#embed "libs/video/renderer/vulkan/shader/gridplane.frag.spv"
};
static const byte alignas(uint32_t) pushcolor_frag[] = {
#embed "libs/video/renderer/vulkan/shader/pushcolor.frag.spv"
};
static const byte alignas(uint32_t) fisheye_frag[] = {
#embed "libs/video/renderer/vulkan/shader/fisheye.frag.spv"
};
static const byte alignas(uint32_t) waterwarp_frag[] = {
#embed "libs/video/renderer/vulkan/shader/waterwarp.frag.spv"
};

typedef struct shaderdata_s {
	const char *name;
	const byte *data;
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
	{ "bsp_gbuf.frag", bsp_gbuf_frag, sizeof (bsp_gbuf_frag) },
	{ "bsp_shadow.vert", bsp_shadow_vert, sizeof (bsp_shadow_vert) },
	{ "bsp_sky.frag", bsp_sky_frag, sizeof (bsp_sky_frag) },
	{ "bsp_turb.frag", bsp_turb_frag, sizeof (bsp_turb_frag) },
	{ "debug.frag", debug_frag, sizeof (debug_frag) },
	{ "entid.frag", entid_frag, sizeof (entid_frag) },
	{ "light_entid.r", light_entid_r, sizeof (light_entid_r) },
	{ "light_splat.r", light_splat_r, sizeof (light_splat_r) },
	{ "light_debug.r", light_debug_r, sizeof (light_debug_r) },
	{ "light_oit.r", light_oit_r, sizeof (light_oit_r) },
	{ "lighting_cascade.r", lighting_cascade_r, sizeof (lighting_cascade_r) },
	{ "lighting_cube.r", lighting_cube_r, sizeof (lighting_cube_r) },
	{ "lighting_none.r", lighting_none_r, sizeof (lighting_none_r) },
	{ "lighting_plane.r", lighting_plane_r, sizeof (lighting_plane_r) },
	{ "compose.r", compose_r, sizeof (compose_r) },
	{ "compose_fwd.r", compose_fwd_r, sizeof (compose_fwd_r) },
	{ "mesh.r", mesh_r, sizeof (mesh_r) },
	{ "mesh_shadow.r", mesh_shadow_r, sizeof (mesh_shadow_r) },
	{ "qskin_fwd.frag", qskin_fwd_frag, sizeof (qskin_fwd_frag) },
	{ "qskin_gbuf.frag", qskin_gbuf_frag, sizeof (qskin_gbuf_frag) },
	{ "output.frag", output_frag, sizeof (output_frag) },
	{ "painter.r", painter_r, sizeof (painter_r) },
	{ "expand.r", expand_r, sizeof (expand_r) },
	{ "queueobj.r", queueobj_r, sizeof (queueobj_r) },
	{ "gizmo.r", gizmo_r, sizeof (gizmo_r) },
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
	} else if (strncmp (shader_path, SHADER, SHADER_SIZE) == 0) {
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
			0, data->size, (const uint32_t *) data->data
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
