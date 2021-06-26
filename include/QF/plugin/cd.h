/*
	QF/plugin/cd.h

	CDAudio plugin data types

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
#ifndef __QF_plugin_cd_h
#define __QF_plugin_cd_h

#include <QF/plugin.h>
#include <QF/qtypes.h>

typedef struct cd_funcs_s {
	void      (*init) (void);
	void      (*cd_f) (void); //
	void      (*pause) (void);
	void      (*play) (int, qboolean);
	void      (*resume) (void);
	void      (*update) (void);
} cd_funcs_t;

typedef struct cd_data_s {
	int         unused;
} cd_data_t;

#endif // __QF_plugin_cd_h
