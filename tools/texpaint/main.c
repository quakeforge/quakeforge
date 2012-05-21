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


#include "texturepaint.h"


void begingl(GtkWidget *glarea)
{
	if (!gtk_gl_area_begingl(GTK_GL_AREA(glarea)))
	{
		gimp_message("gtk_gl_area_begingl failed()!");
		gtk_main_quit();
	}
}

void endgl(GtkWidget *glarea)
{
	gtk_gl_area_endgl(GTK_GL_AREA(glarea));
}

gint expose(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	model_draw((ModelInfo*)data);
	return TRUE;
}

gint resize(GtkWidget *widget, GdkEventConfigure *event,gpointer data)
{
	if (!GTK_WIDGET_REALIZED(widget))
		return TRUE;

	begingl(widget);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0,
		(float) widget->allocation.width / widget->allocation.height,
		0.1, 200.0);
	glMatrixMode(GL_MODELVIEW);

	glViewport(0, 0, widget->allocation.width, widget->allocation.height);

	model_draw((ModelInfo *)data);

	endgl(widget);

	return TRUE;
}


void BindTexture(ModelInfo *mdl,char *texdata,int w,int h)
{

	DEBUG("%s %i",mdl->name,mdl->tex_image);

	glBindTexture(GL_TEXTURE_2D, mdl->texture);
	set_parameter(mdl);

	glTexImage2D(GL_TEXTURE_2D, 0, 3, w, h, 0,
		GL_RGB, GL_UNSIGNED_BYTE, texdata);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glBindTexture(GL_TEXTURE_2D,mdl->texture);
}


gint button_press_event(GtkWidget *widget, GdkEventButton *event,gpointer data)
{
	ModelInfo *mdl;

	mdl=(ModelInfo *)data;

	if (event->type==GDK_2BUTTON_PRESS)
	{
		if (event->button==1)
			on_paint_clicked(NULL,mdl);
		else if (event->button==3)
			on_generate_clicked(NULL,mdl);

	}

	if (event->button == 1)
	{
		mdl->M1_beginx = event->x;
		mdl->M1_beginy = event->y;
		trackball(mdl->lastquat,0,0,0,0);
	} else if (event->button == 2)
	{
		mdl->M2_beginx = event->x;
		mdl->M2_beginy = event->y;
	}  else if (event->button == 3)
	{
		mdl->M3_beginy = event->y;
	}
	return TRUE;
}

gint motion_notify_event(GtkWidget *widget, GdkEventMotion *event,
			 gpointer data)
{
	int x, y;
	int width,height;
	GdkModifierType state;
	ModelInfo *mdl;


	mdl=(ModelInfo *)data;

	width=widget->allocation.width;
	height=widget->allocation.height;

	if (event->is_hint)
	{
		gdk_window_get_pointer(event->window, &x, &y, &state);
	} else
	{
		x=event->x;
		y=event->y;
		state = event->state;
	}

	if ((state & (GDK_BUTTON1_MASK|GDK_BUTTON2_MASK|GDK_BUTTON3_MASK))==0)
		return TRUE;


	if (state & GDK_BUTTON1_MASK)
	{
		trackball(mdl->lastquat,
			(2.0 * mdl->M1_beginx - width) / width,
			(height- 2.0 * mdl->M1_beginy) / height,
			(2.0 * x - width) / width,
			(height- 2.0 * y) / height);
		mdl->M1_beginx = x;
		mdl->M1_beginy = y;
		add_quats(mdl->lastquat,mdl->curquat,mdl->curquat);
	} else if (state & GDK_BUTTON2_MASK)
	{
		mdl->obj_Xpos += (mdl->obj_Zpos / -width) * (x - mdl->M2_beginx);
		mdl->obj_Ypos -= (mdl->obj_Zpos / -height) * (y - mdl->M2_beginy);

		mdl->M2_beginx = x;
		mdl->M2_beginy = y;

	} else if (state & GDK_BUTTON3_MASK)
	{
		mdl->obj_Zpos += (mdl->obj_Zpos / -height) * (y - mdl->M3_beginy);
		mdl->M3_beginy = y;
	}

	model_draw(mdl);

  return TRUE;
}

static void initgl(ModelInfo *mdl)
{
	static int first_initgl=1;
  	GtkWidget *widget;
	GLint data[4];

	widget=mdl->glarea;

	if (first_initgl)
	{
		glGetIntegerv(GL_MAX_TEXTURE_SIZE,data);
		max_tex_size=data[0];

		glGetIntegerv(GL_RED_BITS,data);
		red_bits=data[0];
		glGetIntegerv(GL_GREEN_BITS,data);
		green_bits=data[0];
		glGetIntegerv(GL_BLUE_BITS,data);
		blue_bits=data[0];

		red_shift   = 8-red_bits;
		green_shift = 8-green_bits;
		blue_shift  = 8-blue_bits;

		first_initgl=0;
	}


	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0,
		(float) widget->allocation.width / widget->allocation.height,
		0.1, 200.0);
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnable(GL_DEPTH_TEST);

	glDisable(GL_DITHER);
	glShadeModel(GL_FLAT);
	glEnable(GL_TEXTURE_2D);

	glGenTextures(1,&mdl->texture);

	get_texture(mdl);

	glViewport(0, 0, widget->allocation.width, widget->allocation.height);
}


void on_center_clicked(GtkButton *button, gpointer data)
{
	ModelInfo *mdl;

	if (data)
		mdl=(ModelInfo *)data;
	else
		mdl=dialog->mdl;

	if (mdl)
	{
		mdl->obj_Xpos = 0.0;
		mdl->obj_Ypos = 0.0;
		mdl->obj_Zpos = -2.0;
		trackball(mdl->curquat,0,0,0,0);
		trackball(mdl->lastquat,0,0,0,0);
		model_draw(mdl);
	}
}

void model_new(char *name,Model *model)
{
	int attrList[]=
	{ GDK_GL_RGBA, GDK_GL_DOUBLEBUFFER, GDK_GL_DEPTH_SIZE, 1, GDK_GL_NONE};

	GtkWidget *glarea;
	ModelInfo *mdl_info;
	char str[256];

	mdl_info=g_malloc(sizeof(ModelInfo));
	memset(mdl_info,0,sizeof(ModelInfo));

	mdl_info->name=strdup(name);
	mdl_info->model=model;

	mdl_info->tex_image=-1;
	mdl_info->paint_image=-1;
	mdl_info->idle=-1;

	trackball(mdl_info->curquat,0,0,0,0);
	trackball(mdl_info->lastquat,0,0,0,0);

	mdl_info->obj_Zpos=-2;

	mdl_info->first=0;
	mdl_info->last=mdl_info->model->numframes-1;
	mdl_info->frame=0;
	mdl_info->timer=g_timer_new();


	mdl_info->glwindow=create_glwindow();

	strcpy(str,"3D Window - ");
	strncat(str,name,200);
	gtk_window_set_title(GTK_WINDOW(mdl_info->glwindow),str);
	gtk_object_set_data(GTK_OBJECT(mdl_info->glwindow),MODEL_KEY,mdl_info);

	glarea = gtk_gl_area_new(attrList);
	mdl_info->glarea=glarea;

	if (glarea == NULL) {
		g_print("Can't create GtkGLArea widget\n");
		gtk_exit(1);
	}
	gtk_widget_set_usize(glarea, 200, 200);   /* Minimum size */
	gtk_widget_set_events(glarea,
			GDK_EXPOSURE_MASK |
			GDK_BUTTON_PRESS_MASK |
			GDK_BUTTON_RELEASE_MASK |
			GDK_POINTER_MOTION_MASK |
			GDK_POINTER_MOTION_HINT_MASK);

	gtk_signal_connect(GTK_OBJECT(glarea), "expose_event",
			GTK_SIGNAL_FUNC(expose), mdl_info);

	/* when widgets size changes we want to change glViewport size to match it */
	gtk_signal_connect(GTK_OBJECT(glarea), "configure_event",
		GTK_SIGNAL_FUNC(resize), mdl_info);
	/* virtual trackball */
	gtk_signal_connect(GTK_OBJECT(glarea), "motion_notify_event",
		GTK_SIGNAL_FUNC(motion_notify_event), mdl_info);
	gtk_signal_connect(GTK_OBJECT(glarea), "button_press_event",
		GTK_SIGNAL_FUNC(button_press_event), mdl_info);

	gtk_container_add(GTK_CONTAINER(mdl_info->glwindow), glarea);
	gtk_widget_show(mdl_info->glwindow);
	gtk_widget_show(glarea);
	gtk_widget_realize(glarea);

	dialog->models_list=g_list_append(dialog->models_list,mdl_info);

	update_models_menu(mdl_info);
}

void add_anim_button(GtkWidget *container,int num,char **pix)
{
	GtkStyle *style;
	GdkBitmap *mask;
	GdkPixmap *gdkpix;
	GtkWidget *gtkpix,*button;

	button=gtk_button_new();
	gtk_container_add(GTK_CONTAINER(container),button);
	gtk_widget_realize(button);
	style = gtk_widget_get_style(button);
	gdkpix = gdk_pixmap_create_from_xpm_d(button->window,
                                        &mask,
                                        &style->bg[GTK_STATE_NORMAL],
                                        pix);
	gtkpix = gtk_pixmap_new(gdkpix, mask);
	gtk_widget_show(gtkpix);
	gtk_container_add(GTK_CONTAINER(button),gtkpix);
	gtk_signal_connect(GTK_OBJECT(button),"clicked",
		GTK_SIGNAL_FUNC(on_anim_clicked),GINT_TO_POINTER(num));
	gtk_widget_show(button);
}


void display_model(ModelInfo *mdl,int mode)
{
	GLfloat m[4][4];
	int nextframe;
	int frame;
	float interp;

	DEBUG("%s",mdl->name);


	if (!GTK_WIDGET_REALIZED(mdl->glarea)) return;

	if (mdl->model==NULL) return;

	begingl(mdl->glarea);

	if (!mdl->init)
	{
		initgl(mdl);
		mdl->init = 1;
	}

	if (mode==DRAW_NORMAL)
	{
		glClearColor(.3,.4,.6,0);
	}
	else
	{
		glClearColor(0,0,0,0);
	}

	if (mode==DRAW_WHITE) glDisable(GL_TEXTURE_2D);

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glTranslatef(mdl->obj_Xpos,mdl->obj_Ypos,mdl->obj_Zpos);
	build_rotmatrix(m,mdl->curquat);
	glMultMatrixf(&m[0][0]);
	glRotatef(-90,1,0,0);

	if (mode==DRAW_COORD_S)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,mdl->texture_s);
	}
	else if (mode==DRAW_COORD_T)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,mdl->texture_t);
	}


	if (mdl->last>mdl->first)
	{
		frame=(int)mdl->frame;
		nextframe=(int)mdl->frame+1;
		if (nextframe>=mdl->model->numframes) nextframe=frame;
	}
	else
	{
		nextframe=(int)mdl->frame;
		frame=(int)mdl->frame-1;
		if (frame<0) frame=0;
	}

	interp=mdl->frame-floor(mdl->frame);

	mdl->model->draw(mdl->model,frame,nextframe,interp);

	if (mode!=DRAW_NORMAL)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,mdl->texture);
	}

	endgl(mdl->glarea);
	gtk_gl_area_swapbuffers(GTK_GL_AREA(mdl->glarea));
}

