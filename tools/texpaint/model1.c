/* Texture Paint - a GIMP plugin
 *
 * Copyright (C) 1998  Uwe Maurer <uwe_maurer@t-online.de>
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

#include "texturepaint.h"
#include "model.h"
#include "q1pal.h"

#define MDL_VERSION	6

/*
	MDL (Quake I) Model
*/

typedef struct
{
	gint32 ident;
	gint32 version;
	vec3_t scale;
	vec3_t origin;
	gfloat radius;
	vec3_t offsets;
	gint32 num_skins;
	gint32 skin_width;
	gint32 skin_height;
	gint32 num_verts;
	gint32 num_tris;
	gint32 num_frames;
	gint32 sync_type;
	gint32 flags;
	gfloat size;
} Model1Header;

typedef struct
{
	gint32 onseam;
	gint32 s;
	gint32 t;
} stvert_t;

typedef  struct
{
	gint32 facesfront;
	gint32 v[3];
} mdl_triangle_t;

typedef struct
{
	unsigned char p[3];
	unsigned char  normal;
} vertex_t;

typedef struct
{
	vertex_t min,max;
	char name[16];
} frame_t;

static void destroy(Model *mdl)
{
	g_free(mdl->tri);
	g_free(mdl->vertex);
	g_free(mdl->frames);
	g_free(mdl);
}

static void draw(Model *mdl,int frame,int nextframe,float interp)
{
	int i,j,v;
	vec3_t *vertex;

	gfloat x,y,z;


	frame= (frame % mdl->numframes)*mdl->num_verts;
	nextframe= (nextframe % mdl->numframes)*mdl->num_verts;

	vertex=mdl->vertex;


	glBegin(GL_TRIANGLES);

	for (i=0;i<mdl->num_tris;i++)
	{
		for (j=0;j<3;j++)
		{
			v=mdl->tri[i].v[j];

			x= vertex[frame+v].x +
				(vertex[nextframe+v].x-vertex[frame+v].x)*interp;
			y= vertex[frame+v].y +
				(vertex[nextframe+v].y-vertex[frame+v].y)*interp;
			z= vertex[frame+v].z +
				(vertex[nextframe+v].z-vertex[frame+v].z)*interp;
			glTexCoord2fv(mdl->tri[i].tex[j]);
			glVertex3f(x,y,z);
		}
	}

	glEnd();
}


Model *Model1Load(char *name,FILE *fp)
{
	Model1Header header;
	Model *mdl;
	int i,k,f;
	gint32 dummy;
	guchar *texture;
	gint size;
	gint32 image_id,layer_id;
	GimpPixelRgn rgn;
	GimpDrawable *drawable;
	gint w,h;
	stvert_t *stvert;
	mdl_triangle_t *triangle;
	vertex_t *vertex;
	frame_t frame;
	gfloat x,y,z;
	gint v;
	gfloat xmin,xmax,ymax,ymin,zmax,zmin,scale;
	char filename[300];

	fread(&header,sizeof(header),1,fp);
	if (strncmp((char *)&header.ident,"IDPO",4)!=0) return NULL;
	if (header.version!=MDL_VERSION) return NULL;
	mdl=g_malloc(sizeof(*mdl));
	memset(mdl,0,sizeof(*mdl));

	mdl->num_tris=header.num_tris;
	mdl->num_verts=header.num_verts;

	w=header.skin_width;
	h=header.skin_height;
	size=w*h;
	texture=g_malloc(size);

	image_id=-1;

	for (i=0;i<header.num_skins;i++)
	{
		fread(&dummy,sizeof(dummy),1,fp);
		fread(texture,size,1,fp);
		image_id=gimp_image_new(w,h,GIMP_INDEXED);

		if (i==0)
		{
			strcpy(filename,name);
		}
		else
		{
			sprintf(filename,"%s%i",name,i);
		}
		gimp_image_set_filename(image_id,filename);

		layer_id=gimp_layer_new(image_id,"skin",w,h,GIMP_INDEXED_IMAGE,100,GIMP_NORMAL_MODE);
		gimp_image_add_layer(image_id,layer_id,1);

		gimp_image_set_cmap(image_id,q1pal,256);
		drawable=gimp_drawable_get(layer_id);

		gimp_pixel_rgn_init(&rgn,drawable,0,0,w,h,TRUE,FALSE);
		gimp_pixel_rgn_set_rect(&rgn,texture,0,0,w,h);

		gimp_drawable_detach(drawable);
		gimp_display_new(image_id);
		gimp_image_clean_all(image_id);
	}
	gimp_displays_flush();
	g_free(texture);
	update_images_menu(image_id);

	size=header.num_verts*sizeof(stvert_t);
	stvert=g_malloc(size);
	fread(stvert,size,1,fp);

	size=header.num_tris*sizeof(mdl_triangle_t);
	triangle=g_malloc(size);
	fread(triangle,size,1,fp);

	mdl->tri=g_malloc(header.num_tris*sizeof(triangle_t));
	mdl->vertex=g_malloc(header.num_frames*header.num_verts*sizeof(mdl->vertex[0]));

	size=header.num_verts*sizeof(vertex_t);
	vertex=g_malloc(size);

	mdl->frames=g_malloc(header.num_frames*sizeof(mdl->frames[0]));
	mdl->numframes=header.num_frames;

	xmax=-G_MAXFLOAT;
	ymax=-G_MAXFLOAT;
	zmax=-G_MAXFLOAT;
	xmin=+G_MAXFLOAT;
	ymin=+G_MAXFLOAT;
	zmin=+G_MAXFLOAT;

	for (f=k=0;f<header.num_frames;f++)
	{
		fread(&dummy,sizeof(dummy),1,fp);
		fread(&frame,sizeof(frame),1,fp);

		strncpy(mdl->frames[f].name,frame.name,16);

		fread(vertex,size,1,fp);

		for (i=0;i<header.num_verts;i++,k++)
		{
			x=(gfloat)vertex[i].p[0] * header.scale.x
				+ header.origin.x;
			y=(gfloat)vertex[i].p[1] * header.scale.y
				+ header.origin.y;
			z=(gfloat)vertex[i].p[2] * header.scale.z
				+ header.origin.z;

			mdl->vertex[k].x=x;
			mdl->vertex[k].y=y;
			mdl->vertex[k].z=z;

			if (x>xmax) xmax=x;
			if (x<xmin) xmin=x;
			if (y>ymax) ymax=y;
			if (y<ymin) ymin=y;
			if (z>zmax) zmax=z;
			if (z<zmin) zmin=z;
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



	for (i=0;i<header.num_verts*header.num_frames;i++)
	{
			mdl->vertex[i].x=(mdl->vertex[i].x-x)*scale;
			mdl->vertex[i].y=(mdl->vertex[i].y-y)*scale;
			mdl->vertex[i].z=(mdl->vertex[i].z-z)*scale;
	}

	for (i=0;i<header.num_tris;i++)
	{
		for (k=0;k<3;k++)
		{
			v=triangle[i].v[k];
			x=(gfloat)stvert[v].s/header.skin_width;
			y=(gfloat)stvert[v].t/header.skin_height;

			if (!triangle[i].facesfront && stvert[v].onseam)
			{
				x+=0.5;
			}
			mdl->tri[i].v[k]=v;
			mdl->tri[i].tex[k][0]=x;
			mdl->tri[i].tex[k][1]=y;
		}
	}

	g_free(vertex);
	g_free(stvert);
	g_free(triangle);

	mdl->destroy=destroy;
	mdl->draw=draw;

	return mdl;
}




