/* Texture Paint - a GIMP plugin
 *
 * Copyright (C) 1998  Uwe Maurer <uwe_maurer@t-online.de>
 * 
 * Copyright (C) 1998  Lionel ULMER <bbrox@mygale.org>
 *
 * Based on code Copyright (C) 1997 Trey Harrison <trey@crack.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __MODEL_H__
#define __MODEL_H__

#include <stdio.h>
#include <glib.h>
#include <GL/gl.h>

typedef struct
{
	gfloat x,y,z;
} vec3_t;

typedef struct
{
	gint v[3];
	GLfloat tex[3][2];
} triangle_t;

typedef struct {
  float x, y, z;
  float nx, ny, xz;
} Vertex;

typedef struct {
  float s, t;
} TexInfo;

typedef struct {
  char name[16];
  
  Vertex *vert_table;
} Frame;

typedef struct _Model Model;

struct _Model
{
	/* The number of Frames */
	int numframes;
	Frame *frames;
	
	void (*draw)(Model *,int frame,int nextframe,float interpolation);
	void (*destroy)(Model *);

	/* The GL commands */
	int *glcmds;
	TexInfo *texinfo;

	int num_tris;
	int num_verts;
	triangle_t *tri;
	vec3_t *vertex;
};

Model *Model1Load(char *name,FILE *f);	/* mdl file*/
Model *Model2Load(char *name,FILE *f);  /* md2 file*/

#endif	// __MODEL__H__
