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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <glib.h>

#include "model.h"
#include "texturepaint.h"


void FreeModel(Model *mdl);

typedef struct {
  float x, y, z;
} Vec3;

/*
	MD2 (Quake II) Model
*/
typedef struct
{
  gint32 ident;
  gint32 version;
  gint32 skinwidth;
  gint32 skinheight;
  gint32 framesize;    /* byte size of each frame*/
  gint32 num_skins;
  gint32 num_xyz;
  gint32 num_st;       /* greater than num_xyz for seams*/
  gint32 num_tris;
  gint32 num_glcmds;   /* dwords in strip/fan command list*/
  gint32 num_frames;
  gint32 ofs_skins;    /* each skin is a MAX_SKINNAME string */
  gint32 ofs_st;       /* byte offset from start for stverts */
  gint32 ofs_tris;     /* offset for dtriangles */
  gint32 ofs_frames;   /* offset for first frame */
  gint32 ofs_glcmds;
  gint32 ofs_end;      /* end of file */
} Model2Header;

typedef struct {
  guchar x;
  guchar y;                /* X,Y,Z coordinate, packed on 0-255 */
  guchar z;
  guchar lightnormalindex; /* index of the vertex normal */
} Trivertex;

typedef struct {
  Vec3 scale;          /* multiply byte verts by this */
  Vec3 origin;         /* then add this */
  char name[16];       /* frame name from grabbing */
  Trivertex verts[1];  /* variable sized */
} FrameInfo;

typedef struct
{
	gint16 v[3];
	gint16 st[3];
} d_triangle_t;

typedef struct
{
	gint16 s,t;
} d_stvert_t;

void DrawModel(Model *mdl, int frame, int nextframe, float ratio) {
  int nb_vert;
  long *command;
  Frame *frame1, *frame2;

  /* Keeps the frame value valid */
  frame = frame % mdl->numframes;
  nextframe = nextframe % mdl->numframes;

  /* Gets the frames information */
  frame1 = mdl->frames + frame;
  frame2 = mdl->frames + nextframe;

  /* Do the gl commands */
  command = mdl->glcmds;
  nb_vert = 0;
  while (*command != 0) {
    int num_verts, i;

    /* Determine the command to draw the triangles */
    if (*command > 0) {
      /* Triangle strip */
      num_verts = *command;
      glBegin(GL_TRIANGLE_STRIP);
    } else {
      /* Triangle fan */
      num_verts = -(*command);
      glBegin(GL_TRIANGLE_FAN);
    }
    command++;

    for (i = 0; i < num_verts; i++) {
      Vec3 p;  /* Interpolated point */
      int vert_index;

      /* Grab the vertex index */
      vert_index = *command; command++;

      /* Interpolate */
      p.x = frame1->vert_table[vert_index].x +
	(frame2->vert_table[vert_index].x -
	 frame1->vert_table[vert_index].x) * ratio;
      p.y = frame1->vert_table[vert_index].y +
	(frame2->vert_table[vert_index].y -
	 frame1->vert_table[vert_index].y) * ratio;
      p.z = frame1->vert_table[vert_index].z +
	(frame2->vert_table[vert_index].z -
	 frame1->vert_table[vert_index].z) * ratio;

      glTexCoord2f(mdl->texinfo[nb_vert].s, mdl->texinfo[nb_vert].t);
      glVertex3f(p.x, p.y, p.z);

      nb_vert++;
    }

    glEnd();
  }
}

Model *Model2Load(char *name,FILE *f) {
  int frame;
  Model2Header mdl_header;
  Model *mdl;
  FrameInfo *frames;
  long *glcmds;
  long *command, *cmd_copy;
  TexInfo *texinfo;
  int i,k;
  int num_vertices;
  long offset;
  d_triangle_t *d_tri;
  d_stvert_t *d_stvert;

	gfloat x,y,z;
	gfloat xmin,xmax;
	gfloat ymin,ymax;
	gfloat zmin,zmax,scale;

  /* Failsafe... */
  if (f == NULL)
    return NULL;

  /* In case of a PAK loading */
  offset = ftell(f);

  /* Read the header*/
  fread(&mdl_header, sizeof(Model2Header), 1, f);

  /* Check if this is really a .MD2 file :-) */
  if (strncmp((char *) &(mdl_header.ident), "IDP2", 4))
    return NULL;

  /* Create the model */
  mdl = (Model *) malloc(sizeof(Model));
  memset(mdl, 0, sizeof(Model));

  /* We do not need all the info from the header, just some of it*/
  mdl->numframes = mdl_header.num_frames;

	mdl->draw=DrawModel;
	mdl->destroy=FreeModel;


  /* Reads the GL commands */
  fseek(f, offset + mdl_header.ofs_glcmds, SEEK_SET);
  glcmds = (long *) malloc(mdl_header.num_glcmds * sizeof(long));
  fread(glcmds, mdl_header.num_glcmds * sizeof(long), 1, f);

  /* We keep only the commands and the index in the glcommands. We
     'pre-parse' the texture coordinates */
  /* Do not ask me how I found this formula :-)) */
  num_vertices = ((mdl_header.num_tris + 2 * mdl_header.num_glcmds - 2) / 7);

  mdl->texinfo = (TexInfo *) malloc(sizeof(TexInfo) * num_vertices);
  mdl->glcmds = (long *)
    malloc(sizeof(long) * (mdl_header.num_glcmds - 2 * num_vertices));

  /* Reads the frames */
  fseek(f, offset + mdl_header.ofs_frames, SEEK_SET);
  frames = (FrameInfo *) malloc(mdl_header.framesize * mdl->numframes);
  fread(frames, mdl_header.framesize * mdl->numframes, 1, f);

  /* Converts the FrameInfos to Frames */
  mdl->frames = (Frame *) malloc(sizeof(Frame) * mdl->numframes);

	xmax=-G_MAXFLOAT;
	ymax=-G_MAXFLOAT;
	zmax=-G_MAXFLOAT;
	xmin=+G_MAXFLOAT;
	ymin=+G_MAXFLOAT;
	zmin=+G_MAXFLOAT;

	for (frame = 0; frame < mdl->numframes; frame++)
	{
		FrameInfo *frameinfo;

		/* Gets the frames information */
		frameinfo = (FrameInfo *) ((char *) frames + mdl_header.framesize * frame);
		strcpy(mdl->frames[frame].name, frameinfo->name);
		mdl->frames[frame].vert_table = (Vertex *) malloc(sizeof(Vertex) *
						       mdl_header.num_xyz);


		/* Loads the vertices */
		for (i = 0; i < mdl_header.num_xyz; i++)
		{
			Vertex *p = (mdl->frames[frame].vert_table) + i;

			p->x = (float) frameinfo->verts[i].x *
				frameinfo->scale.x + frameinfo->origin.x;
			p->y =  (float)frameinfo->verts[i].y *
				frameinfo->scale.y + frameinfo->origin.y;
			p->z =  (float)frameinfo->verts[i].z *
				frameinfo->scale.z + frameinfo->origin.z;

			if (p->x<xmin) xmin=p->x;
			if (p->x>xmax) xmax=p->x;
			if (p->y<ymin) ymin=p->y;
			if (p->y>ymax) ymax=p->y;
			if (p->z<zmin) zmin=p->z;
			if (p->z>zmax) zmax=p->z;
		}
	}

	x=(xmax-xmin)/2;
	y=(ymax-ymin)/2;
	z=(zmax-zmin)/2;

	scale=x;
	if (y>scale) scale=y;
	if (z>scale) scale=z;

	if (scale) scale=1/scale; else scale=1;

	x=(xmax+xmin)/2;
	y=(ymax+ymin)/2;
	z=(zmax+zmin)/2;

	for (frame = 0; frame < mdl->numframes; frame++)
	{
		for (i = 0; i < mdl_header.num_xyz; i++)
		{
			Vertex *p = (mdl->frames[frame].vert_table) + i;

			p->x=(p->x-x)*scale;
			p->y=(p->y-y)*scale;
			p->z=(p->z-z)*scale;
		}
	}

  /* Now transform the GL commands */
  command = glcmds;
  cmd_copy = mdl->glcmds;
  texinfo = mdl->texinfo;
  while (*command != 0) {
    int nb_verts, i;

    /* Determine the command to draw the triangles */
    if (*command > 0)
      /* Triangle strip */
      nb_verts = *command;
    else
      /* Triangle fan */
      nb_verts = -(*command);
    *(cmd_copy++) = *(command++);

    for (i = 0; i < nb_verts; i++) {
      float s, t;

      /* Gets the texture information */
      s = *((float *) command); command++;
      t = *((float *) command); command++;
      texinfo->s = s;
      texinfo->t = t;
      texinfo++;

      /* We keep the vertex index */
      *(cmd_copy++) = *(command++);
    }
  }
  /* Do not forget to copy the zero :-) */
  *(cmd_copy++) = *(command++);

	mdl->num_tris=mdl_header.num_tris;
	mdl->tri = malloc(mdl->num_tris*sizeof(*mdl->tri));

	fseek(f, offset + mdl_header.ofs_tris, SEEK_SET);
	d_tri = malloc(mdl->num_tris*sizeof(*d_tri));
	fread(d_tri,mdl->num_tris*sizeof(*d_tri) , 1, f);

	fseek(f, offset + mdl_header.ofs_st, SEEK_SET);
	d_stvert = malloc(mdl_header.num_st*sizeof(*d_stvert));
	fread(d_stvert,mdl_header.num_st*sizeof(*d_stvert) , 1, f);

	for (i=0;i<mdl->num_tris;i++)
	{
		for (k=0;k<3;k++)
		{
			mdl->tri[i].v[k]=d_tri[i].v[k];
			mdl->tri[i].tex[k][0]=(float)d_stvert[d_tri[i].st[k]].s
					/mdl_header.skinwidth;
			mdl->tri[i].tex[k][1]=(float)d_stvert[d_tri[i].st[k]].t
					/mdl_header.skinheight;
		}
	}

	g_free(d_tri);
	g_free(d_stvert);

  /* Clean memory */
  free(frames);
  free(glcmds);

  return mdl;
}

/* Frees the memory used by a model */
void FreeModel(Model *mdl) {
  int frame;

  for (frame = 0; frame < mdl->numframes; frame++)
    free(mdl->frames[frame].vert_table);
  free(mdl->frames);
  free(mdl->glcmds);
  free(mdl->texinfo);
  free(mdl);
}
