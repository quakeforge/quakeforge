/* Texture Paint
 * Plug-in for the GIMP
 *
 * Copyright (C) 1998 Uwe Maurer <uwe_maurer@t-online.de>
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
#include "config.h"

#include "Pixmaps/begin.xpm"
#include "Pixmaps/end.xpm"
#include "Pixmaps/stop.xpm"
#include "Pixmaps/play.xpm"

Dialog *dialog;


int max_tex_size=64;
int red_bits,green_bits,blue_bits;
int red_shift,green_shift,blue_shift;	// 8-red_bits

/* GIMP PLUGIN*/
/*******************************************************************************/

MAIN ()

static void query(void);
static void run(gchar *name,
		gint nparams,
		GimpParam *param,
		gint *nreturn_vals,
		GimpParam **return_vals);

GimpPlugInInfo PLUG_IN_INFO=
{
	NULL,
	NULL,
	query,
	run
};

/*******************************************************************************/

static int round_size(int size,gboolean floor)
{
	static int s[]={1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192};
	int i;

	if (size>=max_tex_size) return max_tex_size;

	if (floor)
	{
		for (i=0;i<=12;i++)
		{
			if (s[i]==size) return size;
			if (s[i]<size && size<s[i+1]) return s[i];
		}
	}
	else
	{
		for (i=0;i<=12;i++)
		{
			if (s[i]==size) return size;
			if (s[i]<size && size<s[i+1]) return s[i+1];
		}
	}

	return max_tex_size;
}

static void set_idle(ModelInfo *mdl)
{
	if (mdl->update_texture || mdl->playing)
	{
		if (mdl->idle<0)
 			mdl->idle=gtk_idle_add_priority(10,(GtkFunction)model_draw,mdl);
	}
	else
	{
		if (mdl->idle>=0)
		{
			gtk_idle_remove(mdl->idle);
			mdl->idle=-1;
		}
	}
}


void set_parameter(ModelInfo *mdl)
{
	GLint v;

	v=(GTK_TOGGLE_BUTTON(dialog->linear)->active) ? GL_LINEAR : GL_NEAREST;

	glBindTexture(GL_TEXTURE_2D, mdl->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, v);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, v);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	v=(GTK_TOGGLE_BUTTON(dialog->fastest)->active) ? GL_FASTEST : GL_NICEST;

	glHint(GL_PERSPECTIVE_CORRECTION_HINT,v);
}


/* radiobuttons */

void on_button_toggled(GtkToggleButton *button,gpointer data)
{
	if (dialog->mdl==NULL) return;

	begingl(dialog->mdl->glarea);
	set_parameter(dialog->mdl);

	dialog->mdl->update_texture=GTK_TOGGLE_BUTTON(dialog->update)->active;
	set_idle(dialog->mdl);

	model_draw(dialog->mdl);
	endgl(dialog->mdl->glarea);
}

void on_update_clicked(GtkButton *button,gpointer data)
{
	ModelInfo *mdl;

	if (data) mdl=(ModelInfo *)data;
	else if (dialog->mdl) mdl=dialog->mdl;
	else return;

	if (get_texture(mdl)<0)
	{
		gimp_message("Select a correct texture first.");
	}

	model_draw(mdl);
}


static void init_special_texture(ModelInfo *mdl,int w,int h)
{
	guchar *image;
	int x,pos;
	int red_mask,green_mask,blue_mask;

	begingl(mdl->glarea);

	red_mask= (1<<red_bits)-1;
	green_mask= (1<<green_bits)-1;
	blue_mask= (1<<blue_bits)-1;

	image=g_malloc(max_tex_size*3);

	pos=0;
	for (x=1;x<=max_tex_size;x++)
	{
		image[pos++]=((x) & red_mask)<<red_shift;
		image[pos++]=((x>>red_bits) & green_mask)<<green_shift;
		image[pos++]=((x>>(red_bits+green_bits)) & blue_mask)<<blue_shift;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT,1);

	glGenTextures(1,&mdl->texture_s);
	glBindTexture(GL_TEXTURE_2D,mdl->texture_s);
	glTexImage2D(GL_TEXTURE_2D,0,3,w,1,0,
		GL_RGB,GL_UNSIGNED_BYTE,image);

	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

	glGenTextures(1,&mdl->texture_t);
	glBindTexture(GL_TEXTURE_2D,mdl->texture_t);
	glTexImage2D(GL_TEXTURE_2D,0,3,1,h,0,
		GL_RGB,GL_UNSIGNED_BYTE,image);

	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

	g_free(image);
	endgl(mdl->glarea);
}


gint model_draw(ModelInfo *mdl)
{
	gdouble frametime;

	DEBUG("%s",mdl->name);

	if (mdl->drawing)
	{
		return TRUE;
	}

	mdl->drawing=TRUE;

	if (mdl->update_texture)
	{
		if (get_texture(mdl)<0)
		{
			mdl->update_texture=FALSE;
			set_idle(mdl);
			if (dialog->mdl==mdl)
				gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(dialog->update),0);
		}
	}

	if (mdl->playing)
	{
		frametime=g_timer_elapsed(mdl->timer,NULL);

		g_timer_start(mdl->timer);

		if (mdl->last > mdl->first)
		{
			mdl->frame+=mdl->fps*frametime;
			if (mdl->frame>(gdouble)mdl->last) mdl->frame=mdl->first;
			if (mdl->frame<(gdouble)mdl->first) mdl->frame=mdl->first;
		}
		else if (mdl->last<mdl->first)
		{
			mdl->frame-=mdl->fps*frametime;
			if (mdl->frame<(gdouble)mdl->last) mdl->frame=mdl->first;
			if (mdl->frame>(gdouble)mdl->first) mdl->frame=mdl->first;
		}
		else
		{
			mdl->frame=mdl->first;
		}

		if (mdl==dialog->mdl)
		{
			gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_range_get_adjustment(GTK_RANGE(dialog->cur_frame))),mdl->frame);

		}
	}

	display_model(mdl,DRAW_NORMAL);
	mdl->drawing=FALSE;
	return TRUE;
}



static void query(void)
{
	static GimpParamDef args[]=
	{
		{GIMP_PDB_INT32,"run_mode","Interactive, non-interactive"},
	};
	gint nargs = sizeof(args) / sizeof(args[0]);

	gimp_install_procedure("plug_in_texture_paint",
		"Paint on a Quake 1/2 Model",
		"",
		"Uwe Maurer <uwe_maurer@t-online.de>",
		"Uwe Maurer, Lionel Ulmer, Janne Löf, Trey Harrison",
		"1998",
		"<Toolbox>/Xtns/Texture Paint",
		"",
		GIMP_EXTENSION,
		nargs,0,
		args,NULL);
}

int get_texture(ModelInfo *mdl)
{
	guchar *tex;
	gint w,h;
	GimpPixelRgn rgn;
	GimpDrawable *drawable;
	gint32 drawable_id;

	DEBUG("%s",mdl->name);

	if (!check_texture(mdl->tex_image))
	{
		glDisable(GL_TEXTURE_2D);
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
		return -1;
	}

	drawable_id=gimp_image_get_active_layer(mdl->tex_image);
	drawable=gimp_drawable_get(drawable_id);

	w=gimp_drawable_width(drawable->id);
	h=gimp_drawable_height(drawable->id);

	tex=g_malloc(3*w*h);
	gimp_pixel_rgn_init(&rgn,drawable,0,0,w,h,FALSE,FALSE);

	gimp_pixel_rgn_get_rect(&rgn,tex,0,0,w,h);
	BindTexture(mdl,tex,w,h);
	g_free(tex);
  	gimp_drawable_detach(drawable);

	glEnable(GL_TEXTURE_2D);
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);


	return 0;
}


void on_load_clicked(GtkButton *button,gpointer data)
{

	gtk_widget_show(dialog->fileselection);
}

void on_cancel_clicked(GtkButton *button,gpointer data)
{
	gtk_widget_hide(dialog->fileselection);
}

void on_ok_clicked(GtkButton *button,gpointer data)
{
	char *filename;
	Model *mdl;
	FILE *fp;
	int len;

	gtk_widget_hide(dialog->fileselection);

	filename=gtk_file_selection_get_filename(
			GTK_FILE_SELECTION(dialog->fileselection));


	if (!filename || (len=strlen(filename))<=4)
	{
		gimp_message("Can't open Model!");
		return;
	}

	if (g_strcasecmp(&filename[len-4],".pak")==0)
	{
		open_pak_file(filename);
	}
	else if ((g_strcasecmp(&filename[len-4],".md2") == 0) || (g_strcasecmp(&filename[len-4],".mdl") == 0))
	{
		fp=fopen(filename,"rb");
		if (fp==NULL || (mdl=model_load(filename,fp))==NULL)
		{
			gimp_message("Can't open file!");
			fclose(fp);
			return;
		}
		fclose(fp);
		model_new(filename,mdl);
	}
	else
	{
		gimp_message("Unknown file!");
	}
}

gboolean on_fileselection_delete(GtkWidget *w,GdkEvent *ev,gpointer data)
{
	gtk_widget_hide(dialog->fileselection);
	return TRUE;
}

void set_image_item(gint32 image_id)
{
	GtkWidget *menu,*item;
	GList *list;
	gint pos,i;
	gint32 image;
	gint32 drawable;
	char str[100];
	int w,h,bpp;

	DEBUG("%i",image_id);

	gtk_widget_set_sensitive(dialog->scale,(image_id>=0));

	str[0]='\0';

	if (image_id>=0)
	{
		drawable=gimp_image_get_active_layer(image_id);
		if (drawable>=0)
		{
			w=gimp_drawable_width(drawable);
			h=gimp_drawable_height(drawable);
			bpp=gimp_drawable_bpp(drawable);

			sprintf(str,"%ix%i, %i bpp",w,h,bpp);

			if (!check_texture(image_id))
			{
				strcat(str,": no texture");
			}
			else
			{
				strcat(str,": texture ok");
			}
			dialog->oldwidth=round_size(w,FALSE);
			dialog->oldheight=round_size(h,FALSE);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->width),dialog->oldwidth);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->height),dialog->oldheight);
		}
		else	strcpy(str,"no active layer");
	}

	if (dialog->mdl)
		DEBUG("%s %i",dialog->mdl->name,dialog->mdl->tex_image);

	if (dialog->mdl && dialog->mdl->tex_image!=image_id)
	{
		dialog->mdl->tex_image=image_id;
		get_texture(dialog->mdl);
		model_draw(dialog->mdl);
	}

	gtk_label_set(GTK_LABEL(dialog->info),str);

	menu=gtk_option_menu_get_menu(GTK_OPTION_MENU(dialog->images_menu));

	list=gtk_container_children(GTK_CONTAINER(menu));

	pos=0;

	for (i=0;list;list=list->next,i++)
	{
		item=GTK_WIDGET(list->data);
		image=GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(item),IMAGE_KEY));
		if (image==image_id) pos=i;
	}

	gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->images_menu),pos);
}

static void update_value(GtkAdjustment *a,gpointer data)
{
	int n;
	ModelInfo *mdl;
	char *name;


	n=GPOINTER_TO_INT(data);
	mdl=dialog->mdl;
	if (!mdl) return;

	switch(n)
	{
		case 1:
			mdl->frame=a->value;
			model_draw(mdl);
			name=mdl->model->frames[(int)mdl->frame].name;
			gtk_label_set(GTK_LABEL(dialog->frame_info2),name);
			break;
		case 2: mdl->first=(int)a->value; break;
		case 3: mdl->last=(int)a->value; break;
		case 4: mdl->fps=a->value; break;

	}

}

void on_anim_clicked(GtkButton *button,gpointer data)
{
	int n;
	ModelInfo *mdl;

	n=GPOINTER_TO_INT(data);
	mdl=dialog->mdl;
	if (!mdl) return;

	switch(n)
	{
		case 1:
			mdl->playing=TRUE;
			set_idle(mdl);
			gtk_widget_set_sensitive(dialog->cur_frame,FALSE);
			g_timer_start(mdl->timer);
			break;
		case 2:
			mdl->playing=FALSE;
			gtk_widget_set_sensitive(dialog->cur_frame,TRUE);
			set_idle(mdl);
			break;
		case 3:
			mdl->frame=mdl->first;
			gtk_adjustment_set_value(GTK_ADJUSTMENT
				(gtk_range_get_adjustment(GTK_RANGE(dialog->cur_frame))),mdl->frame);
			break;
		case 4:
			mdl->frame=mdl->last;
			gtk_adjustment_set_value(GTK_ADJUSTMENT
				(gtk_range_get_adjustment(GTK_RANGE(dialog->cur_frame))),mdl->frame);
			break;
	}
}


void set_model_item(ModelInfo *model)
{
	ModelInfo *mdl,*m;
	int i,pos;
	GtkWidget *menu,*item;
	GList *list;
	char str[256];
	GtkObject *adjustment;

	if (model) DEBUG("%s",model->name);

	menu=gtk_option_menu_get_menu(GTK_OPTION_MENU(dialog->models_menu));

	list=gtk_container_children(GTK_CONTAINER(menu));

	pos=-1;
	mdl=NULL;

	for (i=0;list;list=list->next,i++)
	{
		item=GTK_WIDGET(list->data);
		m=gtk_object_get_data(GTK_OBJECT(item),MODEL_KEY);
		if (m==model)
		{
			pos=i;
			mdl=m;
		}
	}
	DEBUG("%i",pos);
	dialog->mdl=mdl;

	if (mdl)
	{
		gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->models_menu),pos);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(dialog->update),
			dialog->mdl->update_texture);

		update_images_menu(dialog->mdl->tex_image);


		gtk_widget_set_sensitive(dialog->frame_info,TRUE);
		gtk_widget_set_sensitive(dialog->frame_info2,TRUE);
		gtk_widget_set_sensitive(dialog->cur_frame,TRUE);
		gtk_widget_set_sensitive(dialog->start_frame,TRUE);
		gtk_widget_set_sensitive(dialog->end_frame,TRUE);
		gtk_widget_set_sensitive(dialog->fps,TRUE);

		sprintf(str,"%i frames",(int)mdl->model->numframes);
		gtk_label_set(GTK_LABEL(dialog->frame_info),str);

		adjustment=gtk_adjustment_new(mdl->frame,0,mdl->model->numframes-1,1,1,0);
		gtk_range_set_adjustment(GTK_RANGE(dialog->cur_frame),GTK_ADJUSTMENT(adjustment));
		gtk_signal_connect(GTK_OBJECT(adjustment),"value_changed",
			GTK_SIGNAL_FUNC(update_value),GINT_TO_POINTER(1));
		update_value(GTK_ADJUSTMENT(adjustment),GINT_TO_POINTER(1));

		adjustment=gtk_adjustment_new(mdl->first,0,mdl->model->numframes-1,1,1,0);
		gtk_range_set_adjustment(GTK_RANGE(dialog->start_frame),GTK_ADJUSTMENT(adjustment));
		gtk_signal_connect(GTK_OBJECT(adjustment),"value_changed",
			GTK_SIGNAL_FUNC(update_value),GINT_TO_POINTER(2));
		update_value(GTK_ADJUSTMENT(adjustment),GINT_TO_POINTER(2));

		adjustment=gtk_adjustment_new(mdl->last,0,mdl->model->numframes-1,1,1,0);
		gtk_range_set_adjustment(GTK_RANGE(dialog->end_frame),GTK_ADJUSTMENT(adjustment));
		gtk_signal_connect(GTK_OBJECT(adjustment),"value_changed",
			GTK_SIGNAL_FUNC(update_value),GINT_TO_POINTER(3));
		update_value(GTK_ADJUSTMENT(adjustment),GINT_TO_POINTER(3));

		adjustment=gtk_adjustment_new(15,.1,30,1,1,0);
		gtk_range_set_adjustment(GTK_RANGE(dialog->fps),GTK_ADJUSTMENT(adjustment));
		gtk_signal_connect(GTK_OBJECT(adjustment),"value_changed",
			GTK_SIGNAL_FUNC(update_value),GINT_TO_POINTER(4));
		update_value(GTK_ADJUSTMENT(adjustment),GINT_TO_POINTER(4));
	}
	else
	{
		gtk_widget_set_sensitive(dialog->frame_info,FALSE);
		gtk_widget_set_sensitive(dialog->frame_info2,FALSE);
		gtk_widget_set_sensitive(dialog->cur_frame,FALSE);
		gtk_widget_set_sensitive(dialog->start_frame,FALSE);
		gtk_widget_set_sensitive(dialog->end_frame,FALSE);
		gtk_widget_set_sensitive(dialog->fps,FALSE);

		update_images_menu(-1);
	}
}

static void model_activate(GtkWidget *widget)
{
	ModelInfo *mdl;

	mdl=gtk_object_get_data(GTK_OBJECT(widget),MODEL_KEY);

	set_model_item(mdl);

}

void update_models_menu(ModelInfo *mdl)
{
	GtkWidget *menu,*item;
	GList *list;
	GtkWidget *option_menu;
	char *name;

	if (mdl) DEBUG("%s",mdl->name);

	menu=gtk_menu_new();

	list=dialog->models_list;
	option_menu=dialog->models_menu;

	if (g_list_length(list)==0)
	{
		item=gtk_menu_item_new_with_label("none");
		gtk_widget_show(item);
		gtk_menu_append(GTK_MENU(menu),item);
		gtk_widget_set_sensitive(option_menu,FALSE);
	}
	else
	{
		gtk_widget_set_sensitive(option_menu,TRUE);
		for (;list;list=list->next)
		{
			name=((ModelInfo*)list->data)->name;

			item=gtk_menu_item_new_with_label(name);
			gtk_object_set_data(GTK_OBJECT(item),MODEL_KEY,list->data);
			gtk_signal_connect(GTK_OBJECT(item),"activate",GTK_SIGNAL_FUNC(model_activate),NULL);
			gtk_widget_show(item);
			gtk_menu_append(GTK_MENU(menu),item);
		}
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu),menu);
	set_model_item(mdl);
}

static gboolean check_size(int w,int h)
{
	int i;

	for (i=1;i<=max_tex_size;i*=2)
	{
		if (i==w) break;
	}

	if (i>max_tex_size) return FALSE;

	for (i=1;i<=max_tex_size;i*=2)
	{
		if (i==h) break;
	}

	if (i>max_tex_size) return FALSE;
	return TRUE;
}

gboolean check_texture(gint32 image)
{
	gint32 drawable_id;
	gint bpp,w,h;

	if (image<0) return FALSE;

	drawable_id=gimp_image_get_active_layer(image);

	if (drawable_id<0) return FALSE;

	bpp=gimp_drawable_bpp(drawable_id);

	if (bpp!=3) return FALSE;

	w=gimp_drawable_width(drawable_id);
	h=gimp_drawable_height(drawable_id);

	return check_size(w,h);
}

static void image_activate(GtkWidget *item)
{
	gint32 image;

	image=GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(item),IMAGE_KEY));
	set_image_item(image);
}

void update_images_menu(gint32 image_id)
{
	gint32 *images;
	gint nimages,i,pos;
	GtkWidget *item;
	GtkWidget *menu;
	gchar *name;
	gint32 old_image;


	DEBUG("%i",image_id);

	menu=gtk_menu_new();

	images=gimp_image_list(&nimages);

	item=gtk_menu_item_new_with_label("none");
	gtk_object_set_data(GTK_OBJECT(item),IMAGE_KEY,GINT_TO_POINTER(-1));
	gtk_signal_connect(GTK_OBJECT(item),"activate",GTK_SIGNAL_FUNC(image_activate),NULL);
	gtk_widget_show(item);
	gtk_menu_append(GTK_MENU(menu),item);

	pos=0;

	item=GTK_OPTION_MENU(dialog->images_menu)->menu_item;
	if (item) old_image=GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(item),IMAGE_KEY));
	else old_image=-1;

	for (i=0;i<nimages;i++)
	{
		name=gimp_image_get_filename(images[i]);
		item=gtk_menu_item_new_with_label(name);
		gtk_signal_connect(GTK_OBJECT(item),"activate",GTK_SIGNAL_FUNC(image_activate),NULL);
		gtk_object_set_data(GTK_OBJECT(item),IMAGE_KEY,GINT_TO_POINTER(images[i]));
		gtk_widget_show(item);
		gtk_menu_append(GTK_MENU(menu),item);

		if (images[i]==old_image) pos=i+1;

	}

	g_free(images);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(dialog->images_menu),menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->images_menu),pos);

	set_image_item(image_id);
}

void on_images_menu_enter(GtkButton *widget,gpointer data)
{
	if (dialog->mdl)
		update_images_menu(dialog->mdl->tex_image);
	else
		update_images_menu(-1);
}

void on_scale_clicked(GtkButton *b,gpointer data)
{
	gint w,h;
	GtkWidget *item;
	gint32 image_id;
	GimpParam *return_val;
	gint nreturn_val;
	gboolean new_display;


	w=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->width));
	h=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->height));

	item=GTK_OPTION_MENU(dialog->images_menu)->menu_item;
	image_id=GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(item),IMAGE_KEY));

	new_display=FALSE;

	if (GTK_TOGGLE_BUTTON(dialog->new_image)->active)
	{
		return_val=gimp_run_procedure("gimp_channel_ops_duplicate",
				&nreturn_val,GIMP_PDB_IMAGE,image_id,GIMP_PDB_END);
		if (return_val[0].data.d_status!=GIMP_PDB_SUCCESS)
		{
			gimp_destroy_params(return_val,nreturn_val);
			gimp_message("gimp_channel_ops_duplicate failed!");
			return;
		}
		gimp_destroy_params(return_val,nreturn_val);
		image_id=return_val[1].data.d_image;
		new_display=TRUE;
	}

	if (image_id<0)
	{
		gimp_message("Select an image first.");
		return;
	}

	return_val=gimp_run_procedure("gimp_image_scale",
		&nreturn_val,
		GIMP_PDB_IMAGE,image_id,
		GIMP_PDB_INT32,w,
		GIMP_PDB_INT32,h,
		GIMP_PDB_END);

	if (return_val[0].data.d_status!=GIMP_PDB_SUCCESS)
	{
		gimp_destroy_params(return_val,nreturn_val);
		gimp_message("gimp_image_scale failed!");
		return;
	}
	gimp_destroy_params(return_val,nreturn_val);

	if (gimp_image_base_type(image_id)!=GIMP_RGB)
	{
		return_val=gimp_run_procedure("gimp_convert_rgb",
			&nreturn_val,
			GIMP_PDB_IMAGE,image_id,
			GIMP_PDB_END);
		if (return_val[0].data.d_status!=GIMP_PDB_SUCCESS)
		{
			gimp_destroy_params(return_val,nreturn_val);
			gimp_message("gimp_convert_rgb failed!");
			return;
		}
		gimp_destroy_params(return_val,nreturn_val);
	}

	update_images_menu(image_id);

	if (dialog->mdl)
	{
		get_texture(dialog->mdl);
		model_draw(dialog->mdl);
	}

	if (new_display)
	{
		gimp_display_new(image_id);
		gimp_displays_flush();
	}
}

void on_base_texture_clicked(GtkButton *b,gpointer data)
{
	gint w,h;
	gint32 image_id;
	gint32 layer_id;
	GimpDrawable *drawable;
	guchar backgr[3],foregr[3];
	GimpParam *return_val;
	gint nreturn_val;
	gdouble points[4];
	int j,i,k;
	Model *mdl;

	gchar brush[1024];
	gdouble opacity;
	gint32 mode;




	if (dialog->mdl==NULL) return;
	mdl=dialog->mdl->model;

	gimp_palette_get_background(&backgr[0],&backgr[1],&backgr[2]);
	gimp_palette_get_foreground(&foregr[0],&foregr[1],&foregr[2]);

	return_val=gimp_run_procedure("gimp_brushes_get_brush",
				&nreturn_val,GIMP_PDB_END);

	if (return_val[0].data.d_status == GIMP_PDB_SUCCESS)
	{
		strncpy(brush,return_val[1].data.d_string,sizeof(brush));
	}
	else
	{
		brush[0]='\0';
	}
	gimp_destroy_params(return_val,nreturn_val);


	return_val=gimp_run_procedure("gimp_brushes_get_opacity",
				&nreturn_val,GIMP_PDB_END);

	if (return_val[0].data.d_status==GIMP_PDB_SUCCESS)
	{
		opacity=return_val[1].data.d_float;
	}
	else
	{
		opacity=100;
	}
	gimp_destroy_params(return_val,nreturn_val);


	return_val=gimp_run_procedure("gimp_brushes_get_paint_mode",
				&nreturn_val,GIMP_PDB_END);

	if (return_val[0].data.d_status==GIMP_PDB_SUCCESS)
	{
		mode=return_val[1].data.d_int32;
	}
	else
	{
		mode=GIMP_NORMAL_MODE;
	}
	gimp_destroy_params(return_val,nreturn_val);


	gimp_palette_set_foreground(255,255,255);
	gimp_palette_set_background(0,0,0);

	w=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->width));
	h=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog->height));

	image_id=gimp_image_new(w,h,GIMP_RGB);
	gimp_image_set_filename(image_id,"Base Texture");
	layer_id=gimp_layer_new(image_id,"Base Texture",w,h,GIMP_RGB_IMAGE,100,GIMP_NORMAL_MODE);
	gimp_image_add_layer(image_id,layer_id,0);

	drawable=gimp_drawable_get(layer_id);
	gimp_drawable_fill(layer_id,0);

	return_val=gimp_run_procedure("gimp_brushes_set_brush",&nreturn_val,
			GIMP_PDB_STRING,"Circle (01)",GIMP_PDB_END);
	gimp_destroy_params(return_val,nreturn_val);

	return_val=gimp_run_procedure("gimp_brushes_set_paint_mode",&nreturn_val,
			GIMP_PDB_INT32,GIMP_NORMAL_MODE,GIMP_PDB_END);
	gimp_destroy_params(return_val,nreturn_val);

	return_val=gimp_run_procedure("gimp_brushes_set_opacity",&nreturn_val,
			GIMP_PDB_FLOAT,(gdouble)100.0,GIMP_PDB_END);
	gimp_destroy_params(return_val,nreturn_val);

	for (k=0;k<mdl->num_tris;k++)
	{
		j=2;
		for (i=0;i<3;j=i++)
		{
			points[0]=(double)mdl->tri[k].tex[j][0]*w;
			points[1]=(double)mdl->tri[k].tex[j][1]*h;
			points[2]=(double)mdl->tri[k].tex[i][0]*w;
			points[3]=(double)mdl->tri[k].tex[i][1]*h;

			return_val=gimp_run_procedure("gimp-paintbrush",
				&nreturn_val,
				GIMP_PDB_IMAGE,image_id,
				GIMP_PDB_DRAWABLE,layer_id,
				GIMP_PDB_FLOAT,(gdouble)0.0,
				GIMP_PDB_INT32,4,
				GIMP_PDB_FLOATARRAY,&points[0],
				GIMP_PDB_END);
			gimp_destroy_params(return_val,nreturn_val);
		}
	}
	gimp_drawable_detach(drawable);

	gimp_display_new(image_id);
	gimp_image_clean_all(image_id);
	gimp_displays_flush();

	gimp_palette_set_background(backgr[0],backgr[1],backgr[2]);
	gimp_palette_set_foreground(foregr[0],foregr[1],foregr[2]);

	return_val=gimp_run_procedure("gimp_brushes_set_brush",&nreturn_val,
			GIMP_PDB_STRING,brush,GIMP_PDB_END);
	gimp_destroy_params(return_val,nreturn_val);

	return_val=gimp_run_procedure("gimp_brushes_set_paint_mode",&nreturn_val,
			GIMP_PDB_INT32,mode,GIMP_PDB_END);
	gimp_destroy_params(return_val,nreturn_val);

	return_val=gimp_run_procedure("gimp_brushes_set_opacity",&nreturn_val,
			GIMP_PDB_FLOAT,opacity,GIMP_PDB_END);
	gimp_destroy_params(return_val,nreturn_val);
}



static void on_spin_button_changed(GtkSpinButton *button,gpointer data)
{
	int val,oldval;

	oldval=val=gtk_spin_button_get_value_as_int(button);

	switch(GPOINTER_TO_INT(data))
	{
		case 1:
			if (val<dialog->oldwidth)
				val=round_size(val,TRUE);
			else
				val=round_size(val,FALSE);

			dialog->oldwidth=val;
			break;
		case 2:
			if (val<dialog->oldheight)
				val=round_size(val,TRUE);
			else
				val=round_size(val,FALSE);

			dialog->oldheight=val;
			break;

	}

	if (val!=oldval) gtk_spin_button_set_value(button,val);
}

void init_dialog(void)
{
	GtkWidget *w;
	gchar version[256];

	if (dialog) g_free(dialog);

	dialog=g_malloc(sizeof(dialog[0]));
	memset(dialog,0,sizeof(dialog[0]));

	dialog->widget=create_dialog();
	dialog->fileselection=create_fileselection();
	dialog->info=get_widget(dialog->widget,"info");
	dialog->nearest=get_widget(dialog->widget,"nearest");
	dialog->linear=get_widget(dialog->widget,"linear");
	dialog->fastest=get_widget(dialog->widget,"fastest");
	dialog->nicest=get_widget(dialog->widget,"nicest");
	dialog->update=get_widget(dialog->widget,"update");
	dialog->images_menu=get_widget(dialog->widget,"images_menu");
	dialog->models_menu=get_widget(dialog->widget,"models_menu");
	dialog->scale=get_widget(dialog->widget,"scale");
	dialog->new_image=get_widget(dialog->widget,"new_image");

	dialog->frame_info=get_widget(dialog->widget,"frame_info");
	dialog->frame_info2=get_widget(dialog->widget,"frame_info2");
	dialog->cur_frame=get_widget(dialog->widget,"cur_frame");
	dialog->start_frame=get_widget(dialog->widget,"start_frame");
	dialog->end_frame=get_widget(dialog->widget,"end_frame");
	dialog->fps=get_widget(dialog->widget,"fps");

	dialog->update_time=get_widget(dialog->widget,"update_time");

	dialog->width=get_widget(dialog->widget,"width");
	dialog->height=get_widget(dialog->widget,"height");
	gtk_signal_connect(GTK_OBJECT(dialog->width),"changed",
		GTK_SIGNAL_FUNC(on_spin_button_changed),GINT_TO_POINTER(1));
	gtk_signal_connect(GTK_OBJECT(dialog->height),"changed",
		GTK_SIGNAL_FUNC(on_spin_button_changed),GINT_TO_POINTER(2));

	w=get_widget(dialog->widget,"anim_hbox");
	add_anim_button(w,1,play_xpm);
	add_anim_button(w,2,stop_xpm);
	add_anim_button(w,3,begin_xpm);
	add_anim_button(w,4,end_xpm);

	sprintf(version,"Texture Paint %s",PACKAGE_VERSION);
	gtk_label_set(GTK_LABEL(get_widget(dialog->widget,"version")),version);
	dialog->paint_image=-1;
	dialog->texture_drawable=-1;



	gtk_widget_show(dialog->widget);

	update_models_menu(NULL);
}


void on_dialog_destroy(GtkObject *obj,gpointer data)
{

	gtk_main_quit();
}

void on_glwindow_destroy(GtkObject *obj,gpointer data)
{
	ModelInfo *mdl;

	mdl=gtk_object_get_data(obj,MODEL_KEY);
	if (mdl==NULL) return;  /* should never happen*/

	if (mdl->idle>=0) gtk_idle_remove(mdl->idle);

	mdl->model->destroy(mdl->model);

	g_free(mdl->name);

	if (mdl->tex_s) g_free(mdl->tex_s);
	if (mdl->tex_t) g_free(mdl->tex_t);

	g_timer_destroy(mdl->timer);

	g_free(mdl);

	dialog->models_list=g_list_remove(dialog->models_list,mdl);

	if (dialog->models_list)
		update_models_menu((ModelInfo *)dialog->models_list->data);
	else
		update_models_menu(NULL);

}

void on_close_clicked(GtkButton *button,gpointer data)
{
	gtk_main_quit();
}

static void run(gchar *name,
		gint nparams,
		GimpParam *param,
		gint *nreturn_vals,
		GimpParam **return_vals)
{
	static GimpParam val[1];

	gint argc;
	gchar **argv;

	argc=1;
	argv=g_new(gchar *,1);
	argv[0]=g_strdup("Texture Paint");

	gtk_init(&argc, &argv);
	gtk_rc_parse(gimp_gtkrc());
	gdk_set_use_xshm(gimp_use_xshm());

	val[0].type=GIMP_PDB_STATUS;
	val[0].data.d_status=GIMP_PDB_SUCCESS;

	*nreturn_vals=1;
	*return_vals=val;

	init_dialog();

#if 0
{
	FILE *f;
	Model *mdl;

	f=fopen("/home/bob/coding/qview/data/players/male/tris.md2","rb");
	mdl = ModelLoad(f);
	fclose(f);

	model_new("test1",mdl);

	f=fopen("/home/bob/coding/qview/data/players/male/tris.md2","rb");
	mdl = ModelLoad(f);
	fclose(f);

	model_new("test2",mdl);
}
#endif

	gtk_main();
}




static void get_size(ModelInfo *mdl,int *w,int *h)
{
	GLint data[4];

	begingl(mdl->glarea);
	glGetIntegerv(GL_VIEWPORT,data);
	*w=data[2];
	*h=data[3];
	endgl(mdl->glarea);
}

static void get_image(ModelInfo *mdl,guchar **image,int w,int h,int bpp)
{
	guchar *buf,*tmp;
	int y;
	int stride;

	begingl(mdl->glarea);

	buf=g_malloc(w*h*bpp);
	tmp=g_malloc(w*h*3);

	glPixelStorei(GL_PACK_ALIGNMENT,1);
	glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,tmp);

	endgl(mdl->glarea);


	if (bpp==3)
	{
		stride=w*3;

		for (y=0;y<h;y++)
		{
			memcpy(&buf[y*stride],&tmp[(h-y-1)*stride],stride);
		}
	}
	else
	{	int x;

		for (y=0;y<h;y++)
		for (x=0;x<w;x++)
		{
			buf[y*w+x]=tmp[((h-y-1)*w+x)*3];
		}

	}

	g_free(tmp);

	*image=buf;
}

static void copy_to_drawable(ModelInfo *mdl,gint32 drawable_id,int w,int h)
{
	GimpPixelRgn pr;
	GimpDrawable *drawable;
	guchar *image;
	int bpp;

	bpp=gimp_drawable_bpp(drawable_id);
	get_image(mdl,&image,w,h,bpp);

	drawable=gimp_drawable_get(drawable_id);
	gimp_pixel_rgn_init(&pr,drawable,0,0,w,h,TRUE,FALSE);

	gimp_pixel_rgn_set_rect(&pr,image,0,0,w,h);

	gimp_drawable_flush(drawable);
	gimp_drawable_detach(drawable);
	g_free(image);
}

static void decode(gint16 **data,guchar *buf,int w,int h)
{
	int x,y;
	int val;
	int pos;
	gint16 *dest;


	dest=g_malloc(w*h*sizeof(gint16));

	pos=0;
	for (y=0;y<h;y++)
	{
		for (x=0;x<w;x++)
		{
			val=(guint)buf[pos]>>red_shift;
			val+=((guint)buf[pos+1]>>green_shift)<<red_bits;
			val+=((guint)buf[pos+2]>>blue_shift)<<(red_bits+green_bits);
			dest[x+y*w]=val;
			pos+=3;
		}
	}
	*data=dest;
}

static void save_st(ModelInfo *mdl,int w,int h)
{
	guchar *buffer;


	mdl->oldw=w;
	mdl->oldh=h;


	if (mdl->tex_s)	g_free(mdl->tex_s);
	display_model(mdl,DRAW_COORD_S);
	get_image(mdl,&buffer,w,h,3);
	decode(&mdl->tex_s,buffer,w,h);
	g_free(buffer);

	if (mdl->tex_t)	g_free(mdl->tex_t);
	display_model(mdl,DRAW_COORD_T);
	get_image(mdl,&buffer,w,h,3);
	decode(&mdl->tex_t,buffer,w,h);
	g_free(buffer);
}

void on_paint_clicked(GtkButton *button,gpointer data)
{
	gint w,h;
	gint32 image_id;
	gint32 display_id;
	gint32 layer_id,mask_id;
	gint32 bglayer_id;
	guchar r,g,b;

	gint32 *images;
	gint nimages;
	gint32 *layers;
	gint nlayers;
	gint i;
	gboolean new_image;
	ModelInfo *mdl;



	if (data) mdl=(ModelInfo *)data;
	else if (dialog->mdl) mdl=dialog->mdl;
	else return;

	DEBUG("%s",mdl->name);

	if (get_texture(mdl)<0)
	{
		gimp_message("Select a texture image first.");
		return;
	}
	layer_id=gimp_image_get_active_layer(mdl->tex_image);
	w=gimp_drawable_width(layer_id);
	h=gimp_drawable_height(layer_id);

	init_special_texture(mdl,w,h); // FIXME oldw==w

	get_size(mdl,&w,&h);

	save_st(mdl,w,h);

	new_image=TRUE;

	if (mdl->paint_image>=0)
	{
		images=gimp_image_list(&nimages);

		for (i=0;i<nimages;i++)
		{
			if (mdl->paint_image==images[i])
			{
				new_image=FALSE;
				break;
			}
		}
		g_free(images);
	}


	if (new_image)
	{
		image_id=gimp_image_new(w,h,GIMP_RGB);
		mdl->paint_image=image_id;

		gimp_image_set_filename(image_id,"3D Paint");
	}
	else
	{
		image_id=mdl->paint_image;
		layers=gimp_image_get_layers(image_id,&nlayers);

		for (i=0;i<nlayers;i++)
		{
			gimp_image_remove_layer(image_id,layers[i]);
		}

		g_free(layers);

		if (gimp_image_width(image_id)!=w ||
			gimp_image_height(image_id)!=h)
		{
			gimp_image_resize(image_id,w,h,0,0);
		}
	}

	layer_id=gimp_layer_new(image_id,"Texture",w,h,GIMP_RGB_IMAGE,100,GIMP_NORMAL_MODE);

	display_model(mdl,DRAW_NORMAL);
	copy_to_drawable(mdl,layer_id,w,h);

	gimp_layer_add_alpha(layer_id);

	mask_id=gimp_layer_create_mask(layer_id,1);

	display_model(mdl,DRAW_WHITE);
	copy_to_drawable(mdl,mask_id,w,h);

	bglayer_id=gimp_layer_new(image_id,"Background",w,h,GIMP_RGB_IMAGE,100,GIMP_NORMAL_MODE);

	gimp_palette_get_background(&r,&g,&b);
	gimp_palette_set_background(77,102,153);
	gimp_drawable_fill(bglayer_id,0);
	gimp_palette_set_background(r,g,b);

	gimp_image_add_layer(image_id,layer_id,1);
	gimp_image_add_layer_mask(image_id,layer_id,mask_id);
	gimp_image_add_layer(image_id,bglayer_id,2);

	gimp_image_set_active_layer(image_id,layer_id);
	gimp_layer_set_edit_mask(layer_id,FALSE);

	if (new_image)
	{
		display_id=gimp_display_new(image_id);
	}
	gimp_image_clean_all(image_id);
	gimp_displays_flush();

	display_model(mdl,DRAW_NORMAL);

}

void on_generate_clicked(GtkButton *button,gpointer data)
{
	gint32 drawable_id,image_id;
	guchar *src;
	guchar *dest;

	gint32 *buffer;

	GimpPixelRgn pr;
	GimpDrawable *drawable;
	gint w,h;
	gint src_w,src_h;
	gint x,y;
	gint s,t;
	gint bpp;
	gint pos1,pos2;
	gint div;
	ModelInfo *mdl;
	char str[256];


	if (data) mdl=(ModelInfo *)data;
	else if (dialog->mdl) mdl=dialog->mdl;
	else return;

	if (mdl->paint_image<0)
	{
		gimp_message("Click on the \"3D Paint\" button first.");
		return;
	}

	image_id=mdl->paint_image;

	drawable_id=gimp_image_get_active_layer(image_id);

	if (drawable_id<0)
	{
		gimp_message("Can't get active layer!");
		return;
	}

	src_w=gimp_drawable_width(drawable_id);
	src_h=gimp_drawable_height(drawable_id);
	bpp=gimp_drawable_bpp(drawable_id);

	if (src_w!=mdl->oldw || src_h!=mdl->oldh || bpp!=4)
	{
		sprintf(str,"The image must be %ix%i (4 bpp)!",mdl->oldw,mdl->oldh);
		gimp_message(str);
		return;
	}
	if (!check_texture(mdl->tex_image))
	{
		gimp_message("check_texture() failed!");
		return;
	}

	drawable=gimp_drawable_get(drawable_id);

	src=g_malloc(4*src_w*src_h);

	gimp_pixel_rgn_init(&pr,drawable,0,0,src_w,src_h,FALSE,FALSE);
	gimp_pixel_rgn_get_rect(&pr,src,0,0,src_w,src_h);

	gimp_drawable_flush(drawable);
	gimp_drawable_detach(drawable);

	drawable_id=gimp_image_get_active_layer(mdl->tex_image);

	drawable=gimp_drawable_get(drawable_id);

	w=gimp_drawable_width(drawable->id);
	h=gimp_drawable_height(drawable->id);

	dest=g_malloc(3*w*h);

	buffer=g_malloc(w*h*sizeof(gint32[4]));

	memset(buffer,0,w*h*sizeof(gint32[4]));

	gimp_pixel_rgn_init(&pr,drawable,0,0,w,h,TRUE,FALSE);
	gimp_pixel_rgn_get_rect(&pr,dest,0,0,w,h);


	for (y=0;y<src_h;y++)
	{
		for (x=0;x<src_w;x++)
		{
			s=mdl->tex_s[x+y*src_w]-1;
			t=mdl->tex_t[x+y*src_w]-1;

			if (s>0 && t>0)
			{
				pos1=(s+t*w)*4;
				pos2=(x+y*src_w)*4;

				buffer[pos1]+=src[pos2];
				buffer[pos1+1]+=src[pos2+1];
				buffer[pos1+2]+=src[pos2+2];
				buffer[pos1+3]++;
			}
		}
	}

	for (pos1=pos2=y=0;y<h;y++)
	{
		for (x=0;x<w;x++)
		{
			div=buffer[pos1+3];
			if (div)
			{
				dest[pos2]=buffer[pos1]/div;
				dest[pos2+1]=buffer[pos1+1]/div;
				dest[pos2+2]=buffer[pos1+2]/div;
			}
			pos1+=4;
			pos2+=3;
		}
	}

	gimp_pixel_rgn_set_rect(&pr,dest,0,0,w,h);
	gimp_drawable_flush(drawable);
	gimp_drawable_detach(drawable);

	begingl(mdl->glarea);
	BindTexture(mdl,dest,w,h);
	endgl(mdl->glarea);

	display_model(mdl,DRAW_NORMAL);

	g_free(src);
	g_free(dest);
	g_free(buffer);

	gimp_drawable_update(drawable_id,0,0,w,h);
	gimp_displays_flush();
}
