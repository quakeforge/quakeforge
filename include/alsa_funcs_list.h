/*
	alsa_funcs_list.h

	QF ALSA function list

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2002/4/19

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

#ifndef QF_ALSA_NEED
#define QF_ALSA_NEED(ret, func, params)
#define UNDEF_QF_ALSA_NEED
#endif

QF_ALSA_NEED (int, snd_pcm_close, (snd_pcm_t *pcm))
QF_ALSA_NEED (int, snd_pcm_hw_params, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params))
QF_ALSA_NEED (int, snd_pcm_hw_params_any, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params))
#if SND_LIB_MAJOR < 1 && SND_LIB_MINOR >=9 && SND_LIB_SUBMINOR < 8
QF_ALSA_NEED (snd_pcm_sframes_t, snd_pcm_hw_params_get_buffer_size, (const snd_pcm_hw_params_t *params))
QF_ALSA_NEED (snd_pcm_sframes_t, snd_pcm_hw_params_get_period_size, (const snd_pcm_hw_params_t *params, int *dir))
QF_ALSA_NEED (int, snd_pcm_hw_params_set_access, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t val))
QF_ALSA_NEED (snd_pcm_uframes_t, snd_pcm_hw_params_set_period_size_near, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int *dir))
QF_ALSA_NEED (unsigned int, snd_pcm_hw_params_set_rate_near, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir))
#else
QF_ALSA_NEED (int, snd_pcm_hw_params_get_buffer_size, (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val))
QF_ALSA_NEED (int, snd_pcm_hw_params_set_buffer_size, (snd_pcm_t *pcm, const snd_pcm_hw_params_t *params, snd_pcm_uframes_t val))
QF_ALSA_NEED (int, snd_pcm_hw_params_get_period_size, (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir))
QF_ALSA_NEED (int, snd_pcm_hw_params_set_access, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t access))
QF_ALSA_NEED (int, snd_pcm_hw_params_set_period_size_near, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir))
QF_ALSA_NEED (int, snd_pcm_hw_params_set_rate_near, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir))
#endif
QF_ALSA_NEED (int, snd_pcm_hw_params_set_channels, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val))
QF_ALSA_NEED (int, snd_pcm_hw_params_set_format, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val))
QF_ALSA_NEED (size_t, snd_pcm_hw_params_sizeof, (void))
QF_ALSA_NEED (int, snd_pcm_mmap_begin, (snd_pcm_t *pcm, const snd_pcm_channel_area_t **areas, snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames))
QF_ALSA_NEED (int, snd_pcm_avail_update, (snd_pcm_t *pcm))
QF_ALSA_NEED (snd_pcm_sframes_t, snd_pcm_mmap_commit, (snd_pcm_t *pcm, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames))
QF_ALSA_NEED (int, snd_pcm_open, (snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode))
QF_ALSA_NEED (int, snd_pcm_pause, (snd_pcm_t *pcm, int enable))
QF_ALSA_NEED (int, snd_pcm_start, (snd_pcm_t *pcm))
QF_ALSA_NEED (snd_pcm_state_t, snd_pcm_state, (snd_pcm_t *pcm))
QF_ALSA_NEED (int, snd_pcm_sw_params, (snd_pcm_t *pcm, snd_pcm_sw_params_t *params))
QF_ALSA_NEED (int, snd_pcm_sw_params_current, (snd_pcm_t *pcm, snd_pcm_sw_params_t *params))
QF_ALSA_NEED (int, snd_pcm_sw_params_set_start_threshold, (snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val))
QF_ALSA_NEED (int, snd_pcm_sw_params_set_stop_threshold, (snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val))
QF_ALSA_NEED (size_t, snd_pcm_sw_params_sizeof, (void))
QF_ALSA_NEED (const char *, snd_strerror, (int errnum))

QF_ALSA_NEED (int, snd_async_add_pcm_handler, (snd_async_handler_t **handler, snd_pcm_t *pcm, snd_async_callback_t callback, void *private_data))
QF_ALSA_NEED (snd_pcm_t *, snd_async_handler_get_pcm, (snd_async_handler_t *handler))
QF_ALSA_NEED (void *, snd_async_handler_get_callback_private, (snd_async_handler_t *handler))
QF_ALSA_NEED (int, snd_async_del_handler, (snd_async_handler_t *handler))

#ifdef UNDEF_QF_ALSA_NEED
#undef QF_ALSA_NEED
#undef UNDEF_QF_ALSA_NEED
#endif
