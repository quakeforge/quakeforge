/*
	gl_ngraph.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/sys.h"

#include "glquake.h"
#include "r_cvar.h"

extern byte 	*draw_chars;	// 8*8 graphic characters
extern qboolean lighthalf;

extern cvar_t	*r_netgraph;
extern cvar_t	*r_netgraph_alpha;
extern cvar_t	*r_netgraph_box;

static int graph_index;
static int graph_size[NUM_GRAPH_TEXTURES];
static int graph_width[NUM_GRAPH_TEXTURES];
static byte *graph_texels[NUM_GRAPH_TEXTURES];
int 	graph_texture[NUM_GRAPH_TEXTURES];

int
R_InitGraphTextures (int base)
{
	int i;

	for (i = 0; i < NUM_GRAPH_TEXTURES; i++)
		graph_texture[i] = base++;
	return base;
}

void
R_LineGraph (int x, int y, int *h_vals, int count)
{
	int         i, j;
	int         h;
	int         s;
	byte        color;
	int			size;
	byte        *dest;

	if (!count)
		return;

	s = r_graphheight->int_val;

	size = 2 * count;
	if (size > graph_size[graph_index]) {
		graph_size[graph_index] = size;
		graph_texels[graph_index] = realloc (graph_texels[graph_index], size);
	}
	graph_width[graph_index] = count;

	if (!graph_texels[graph_index])
		Sys_Error ("R_LineGraph: failed to allocate texture buffer\n");

	i = 0;
	while (count--) {
		dest = graph_texels[graph_index] + i++;

		h = *h_vals++;

		if (h == 10000)
			color = 0x6f;					// yellow
		else if (h == 9999)
			color = 0x4f;					// red
		else if (h == 9998)
			color = 0xd0;					// blue
		else
			color = 0xfe;					// white

		if (h > s)
			h = s;

		for (j = 0; j < h; j++, dest += graph_width[graph_index])
			dest[0] = color;

		for (; j < s; j++, dest += graph_width[graph_index])
			dest[0] = 0xff;
	}

	glBindTexture (GL_TEXTURE_2D, graph_texture[graph_index]);

	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format,
				  graph_width[graph_index], s, 0, GL_RGBA,
				  GL_UNSIGNED_BYTE, graph_texels[graph_index]);

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x + graph_width[graph_index], y);
	glTexCoord2f (1, 1);
	glVertex2f (x + graph_width[graph_index], y - s);
	glTexCoord2f (0, 1);
	glVertex2f (x, y - s);
	glEnd ();

	glColor3ubv (lighthalf_v);

	graph_index = (graph_index + 1) % NUM_GRAPH_TEXTURES;
}
