/*
	plugin.h

	QuakeForge plugin API structures and prototypes

	Copyright (C) 2001 Jeff Teunissen <deek@dusknet.dhs.org>

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

#ifndef __QF_plugin_h
#define __QF_plugin_h

/** \defgroup plugin Plugins
	\ingroup utils
*/
///@{

#define QFPLUGIN_VERSION	"1.0"

#include <QF/qtypes.h>

#ifdef STATIC_PLUGINS
#define PLUGIN_INFO(type,name) plugin_t *type##_##name##_PluginInfo (void); __attribute__((const)) plugin_t * type##_##name##_PluginInfo (void)
#else
#define PLUGIN_INFO(type,name) plugin_t *PluginInfo (void); __attribute__((visibility ("default"),const)) plugin_t *PluginInfo (void)
#endif

typedef enum {
	qfp_null = 0,	// Not real
	qfp_input,		// Input (pointing devices, joysticks, etc)
	qfp_cd,			// CD Audio
	qfp_console,	// Console `driver'
	qfp_snd_output,	// Sound output (OSS, ALSA, Win32)
	qfp_snd_render,	// Sound mixing
	qfp_vid_render,	// Video renderer
} plugin_type_t;

typedef struct plugin_funcs_s {
	struct general_funcs_s *general;
	struct input_funcs_s	*input;
	struct cd_funcs_s		*cd;
	struct console_funcs_s	*console;
	struct snd_output_funcs_s	*snd_output;
	struct snd_render_funcs_s	*snd_render;
	struct vid_render_funcs_s	*vid_render;
} plugin_funcs_t;

typedef struct plugin_data_s {
	struct general_data_s	*general;
	struct input_data_s	*input;
	struct cd_data_s		*cd;
	struct console_data_s	*console;
	struct snd_output_data_s	*snd_output;
	struct snd_render_data_s	*snd_render;
	struct vid_render_data_s	*vid_render;
} plugin_data_t;

typedef struct plugin_s {
	plugin_type_t	type;
	void			*handle;
	const char		*api_version;
	const char		*plugin_version;
	const char		*description;
	const char		*copyright;
	plugin_funcs_t	*functions;
	plugin_data_t	*data;
	const char		*full_name;
} plugin_t;

/*
	General plugin info return function type
*/
typedef plugin_t *(*plugin_info_t) (void);

typedef struct plugin_list_s {
	const char *name;
	plugin_info_t info;
} plugin_list_t;

/*
	Plugin system variables
*/
extern char *fs_pluginpath;

/*
	Function prototypes
*/
plugin_t *PI_LoadPlugin (const char *, const char *);
qboolean PI_UnloadPlugin (plugin_t *);
void PI_RegisterPlugins (plugin_list_t *);
void PI_Init (void);

///@}

#endif//__QF_plugin_h
