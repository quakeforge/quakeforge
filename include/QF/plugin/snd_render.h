/*
	QF/plugin/snd_render.h

	Sound Renderer plugin data types

	Copyright (C) 2001 Jeff Teunissen <deek@quakeforge.net>

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
#ifndef __QF_plugin_snd_render_h
#define __QF_plugin_snd_render_h

#include <QF/plugin.h>
#include <QF/qtypes.h>
#include <QF/simd/types.h>

struct sfx_s;
struct transform_s;

typedef struct snd_render_funcs_s {
	void      (*init) (void);
	void      (*ambient_off) (void);
	void      (*ambient_on) (void);
	void      (*static_sound) (struct sfx_s *sfx, vec4f_t origin, float vol, float attenuation);
	void      (*start_sound) (int entnum, int entchannel, struct sfx_s *sfx, const vec4f_t, float vol, float attenuation);
	void      (*local_sound) (const char *s);
	void      (*stop_sound) (int entnum, int entchannel);

	struct channel_s *(*alloc_channel) (void);
	void      (*channel_free) (struct channel_s *chan);
	int       (*channel_set_sfx) (struct channel_s *chan, struct sfx_s *sfx);
	void      (*channel_set_paused) (struct channel_s *chan, int paused);
	void      (*channel_set_looping) (struct channel_s *chan, int looping);
	enum chan_state_e (*channel_get_state) (struct channel_s *chan);
	void      (*channel_set_volume) (struct channel_s *chan, float volume);

	struct sfx_s *(*precache_sound) (const char *sample);
	struct sfx_s *(*load_sound) (const char *name);

	void      (*update) (struct transform_s *ear,
						 const byte *ambient_sound_levels);
	void      (*stop_all_sounds) (void);
	void      (*extra_update) (void);
	void      (*block_sound) (void);
	void      (*unblock_sound) (void);
} snd_render_funcs_t;

typedef struct snd_render_data_s {
	double     *host_frametime;
	int        *viewentity;

	unsigned   *soundtime;
	unsigned   *paintedtime;
	struct plugin_s *output;
} snd_render_data_t;

#endif // __QF_plugin_snd_render_h
