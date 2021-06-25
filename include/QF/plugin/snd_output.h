/*
	QF/plugin/sound.h

	Sound Output plugin data types

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
#ifndef __QF_plugin_snd_output_h
#define __QF_plugin_snd_output_h

#include <QF/plugin.h>
#include <QF/qtypes.h>

struct snd_s;

typedef struct snd_output_funcs_s {
	int       (*init) (struct snd_s *snd);
	void      (*shutdown) (struct snd_s *snd);
	int       (*get_dma_pos) (struct snd_s *snd);
	void      (*submit) (struct snd_s *snd);
	void      (*block_sound) (struct snd_s *snd);
	void      (*unblock_sound) (struct snd_s *snd);
} snd_output_funcs_t;

typedef enum {
	som_push,	// synchronous io (mixer pushes data to driver)
	som_pull,	// asynchronous io (driver pulls data from mixer)
} snd_output_model_t;

typedef struct snd_output_data_s {
	unsigned   *soundtime;
	unsigned   *paintedtime;
	snd_output_model_t model;
} snd_output_data_t;

#endif // __QF_plugin_snd_output_h
