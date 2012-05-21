/* Texture Paint
 * Plug-in for the GIMP
 *
 * Copyright (C) 1998 Uwe Maurer <uwe_maurer@t-online.de>
 *
 * Based on GiMd2Viewer-0.1 (Quake2 model viewer) by
 * Copyright (C) 1998  Lionel ULMER <bbrox@mygale.org>
 * Copyright (C) 1997-1998 Janne Löf <jlof@student.oulu.fi>
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
#ifndef __TEXTUREPAINT_H_
#define __TEXTUREPAINT_H_

#include <math.h>
#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <gtkgl/gtkglarea.h>


#include "trackball.h"
#include "model.h"
#include "gladesig.h"
#include "gladesrc.h"

#define DRAW_NORMAL	0
#define DRAW_WHITE	1
#define DRAW_COORD_S	2
#define DRAW_COORD_T	3


#define IMAGE_KEY	"image_id"	/* for gtk_object_set_data */
#define MODEL_KEY	"model"

#if 0

#define DEBUG(fmt,x...) printf(__FUNCTION__ " %i : " fmt "\n" , __LINE__ , ##x);


#else

#define DEBUG(fmt,x...) ;

#endif


typedef struct
{
	char *name;

	GtkWidget *glwindow;
	GtkWidget *glarea;

	Model *model;

	int init;

	gint32 tex_image;
	gint32 paint_image;

	GLint texture;
	GLint texture_s;
	GLint texture_t;

	gint16 *tex_s;
	gint16 *tex_t;

	int oldw,oldh;

	gint idle;

	gint x1,x2,y1,y2;	/* texture drawable*/
	gint w,h;		/* x2-x1+1, y2-y1+1 */

	gboolean update_texture;

	gboolean drawing;

	/*Animation*/
	GTimer *timer;
	gboolean playing;
	gdouble fps;
	gdouble frame;
	int first;
	int last;


	/* Trackball */
	float M1_beginx, M1_beginy; /* Last recorded mouse position */
	float curquat[4];           /* Current quaternion */
	float lastquat[4];          /* Last quaternion */

	/* Object movement */
	float obj_Xpos;
	float obj_Ypos;
	float obj_Zpos;
	int M2_beginx, M2_beginy, M3_beginy;
} ModelInfo;

typedef struct
{
	GtkWidget *widget;
	GtkWidget *info;
	GtkWidget *nearest;
	GtkWidget *linear;
	GtkWidget *fastest;
	GtkWidget *nicest;
	GtkWidget *update;
	GtkWidget *update_time;
	GtkWidget *fileselection;
	GtkWidget *images_menu;
	GtkWidget *models_menu;
	GtkWidget *scale;
	GtkWidget *new_image;

	GtkWidget *frame_info;
	GtkWidget *frame_info2;
	GtkWidget *cur_frame;
	GtkWidget *start_frame;
	GtkWidget *end_frame;
	GtkWidget *fps;

	GtkWidget *width,*height;
	int oldwidth,oldheight;

	gint32  texture_drawable;
	gint32	paint_image;

	GList *models_list;

	ModelInfo *mdl;

} Dialog;

extern Dialog *dialog;

/* open.c */
void open_pak_file(char *file);

extern int max_tex_size;
extern int red_bits,green_bits,blue_bits;
extern int red_shift,green_shift,blue_shift;	// 8-red_bits


gint model_draw(ModelInfo *);
Model *model_load(char *name,FILE *fp);

void begingl(GtkWidget *);
void endgl(GtkWidget *);
void set_parameter(ModelInfo *mdl);
void BindTexture(ModelInfo *mdl,char *texdata,int w,int h);
void add_anim_button(GtkWidget *container,int num,char **pix);
int get_texture(ModelInfo *mdl);
void model_new(char *name,Model *mdl);
void on_anim_clicked(GtkButton *,gpointer);
void update_models_menu(ModelInfo *mdl);
void display_model(ModelInfo *,int mode);
gboolean check_texture(gint32 image);
void update_images_menu(gint32 image_id);
void on_paint_clicked(GtkButton *button,gpointer data);


#endif
