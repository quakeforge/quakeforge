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

	$Id$
*/

#ifndef __QF_plugin_h_
#define __QF_plugin_h_

#define QFPLUGIN_VERSION	"1.0"

#define QFPLUGIN

#include <QF/qtypes.h>
#include <QF/plugin/cd.h>
#include <QF/plugin/general.h>
#include <QF/plugin/input.h>
#include <QF/plugin/sound.h>

typedef enum {
	qfp_null = 0,	// Not real
	qfp_input,		// Input (pointing devices, joysticks, etc)
	qfp_cd,			// CD Audio
	qfp_sound,		// Wave output (OSS, ALSA, Win32)
} plugin_type_t;

typedef struct plugin_funcs_s {
	general_funcs_t *general;
	input_funcs_t	*input;
	cd_funcs_t		*cd;
	sound_funcs_t	*sound;
} plugin_funcs_t;

typedef struct plugin_data_s {
	general_data_t	*general;
	input_data_t	*input;
//	cd_data_t		*cd;
	sound_data_t	*sound;
} plugin_data_t;

typedef struct plugin_s {
	plugin_type_t	type;
	void			*handle;
	char			*api_version;
	char			*plugin_version;
	char			*description;
	char			*copyright;
	plugin_funcs_t	*functions;
	plugin_data_t	*data;
} plugin_t;

/*
	General plugin info return function type
*/
typedef plugin_t * (*P_PluginInfo) (void);

/*
	Plugin system variables
*/
extern struct cvar_s	*fs_pluginpath;

/*
	Function prototypes
*/
plugin_t *PI_LoadPlugin (char *, char *);
qboolean PI_UnloadPlugin (plugin_t *);
void PI_Init (void);
void PI_Shutdown (void);

#endif	// __QF_plugin_h_
