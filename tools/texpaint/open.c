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

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libgimp/gimp.h>
#include "texturepaint.h"
#include "model.h"
#include "pack.h"

typedef struct
{
	GtkWidget *widget;
	GtkWidget *tree;
	FILE *fp;
} Data;


void model_new(char *,Model *mdl);
void update_images_menu(gint32 );

void on_pakdialog_destroy(GtkObject *obj,gpointer data)
{
	Data *d;
	d=(Data *)data;

	fclose(d->fp);
	g_free(d);
}

void on_pak_close_clicked(GtkWidget *w,gpointer data)
{
  	gtk_widget_destroy(GTK_WIDGET(data));
}


void on_pak_open_clicked(GtkWidget *widget,gpointer data)
{
	GList *sel;
	EntryInfo *entry;
	Data *d;
	Model *mdl;
	FILE *tmp;
	GtkWidget *item;
	guchar *buf;
	char name[256],*title;
	gint32 image_id;

	GimpParam *return_vals;
	gint nreturn_vals;

	d=(Data *)data;

	sel=GTK_TREE(d->tree)->selection;

	if (sel)
		item=GTK_WIDGET(sel->data);
	else
		item=widget;


	entry=GetInfo(item);
	if (entry==NULL) return;

	fseek(d->fp,entry->offset,SEEK_SET);

	gtk_label_get(GTK_LABEL(GTK_BIN(item)->child),&title);

	mdl=model_load(title,d->fp);


	if (mdl)
	{
		model_new(title,mdl);
	}
	else
	{
		tmp = tmpfile();
		if (tmp==NULL) return;

		fseek(d->fp,entry->offset,SEEK_SET);

		buf=g_malloc(entry->size);
		fread(buf,1,entry->size,d->fp);
		fwrite(buf,1,entry->size,tmp);
		fclose(tmp);
		g_free(buf);

		return_vals=gimp_run_procedure("gimp_file_load",
				&nreturn_vals,
				GIMP_PDB_INT32,GIMP_RUN_INTERACTIVE,
				GIMP_PDB_STRING,name,
				GIMP_PDB_STRING,name, GIMP_PDB_END);

		if (return_vals[0].data.d_status==GIMP_PDB_SUCCESS)
		{
			image_id=return_vals[1].data.d_image;
			gimp_image_set_filename(image_id,title);
			update_images_menu(image_id);
			gimp_display_new(image_id);
			gimp_displays_flush();
		}

		gimp_destroy_params(return_vals,nreturn_vals);
		remove(name);
	}
}

void on_item_clicked(GtkWidget *widget,GdkEvent *ev,gpointer data)
{
	if (ev->type==GDK_2BUTTON_PRESS)
	{
		on_pak_open_clicked(widget,data);
	}
}

void open_pak_file(char *file)
{
	GtkWidget *dialog;
	Data *data;

	dialog=create_pakdialog();

	data=g_malloc(sizeof(data[0]));
	data->widget=dialog;

	gtk_label_set(GTK_LABEL(get_widget(dialog,"filename")),file);


	gtk_signal_connect(GTK_OBJECT(dialog),"destroy",
		GTK_SIGNAL_FUNC(on_pakdialog_destroy),data);

	gtk_signal_connect(GTK_OBJECT(get_widget(dialog,"close")),"clicked",
		GTK_SIGNAL_FUNC(on_pak_close_clicked),dialog);

	gtk_signal_connect(GTK_OBJECT(get_widget(dialog,"open")),"clicked",
		GTK_SIGNAL_FUNC(on_pak_open_clicked),data);

	data->fp=fopen(file,"rb");
	if (data->fp==NULL)
	{
		gimp_message("Can't open PAK-file!");
		gtk_widget_destroy(dialog);
		return;
	}

	SetItemParams(GTK_SIGNAL_FUNC(on_item_clicked),data);
	data->tree=OpenPAK(data->fp,"pakfile");

	if (data->tree==NULL)
	{
		gimp_message("File is no PAK-file!");
		gtk_widget_destroy(dialog);
		return;
	}

	gtk_widget_show(data->tree);
	gtk_container_add(GTK_CONTAINER(get_widget(dialog,"scrolledwindow")),
		data->tree);
	gtk_widget_show(dialog);
}





Model * model_load(char *name,FILE *fp)
{
	int n;

	n=strlen(name);

	if (n<=4) return NULL;

	if (g_strcasecmp(&name[n-4],".md2")==0)
	{
		return Model2Load(name,fp);
	}
	else if (g_strcasecmp(&name[n-4],".mdl")==0)
	{
		return Model1Load(name,fp);
	}

	return NULL;
}
