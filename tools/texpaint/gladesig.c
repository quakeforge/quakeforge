/*  Note: You are free to use whatever license you want.
    Eventually you will be able to edit it within Glade. */

/*  <PROJECT NAME>
 *  Copyright (C) <YEAR> <AUTHORS>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <gtk/gtk.h>
#include "gladesrc.h"
#include "gladesig.h"

int
main (int argc, char *argv[])
{
  GtkWidget *glwindow;
  GtkWidget *fileselection;
  GtkWidget *pakdialog;
  GtkWidget *dialog;

  gtk_set_locale ();
  gtk_init (&argc, &argv);

  /*
   * The following code was added by Glade to create one of each component
   * (except popup menus), just so that you see something after building
   * the project. Delete any components that you don't want shown initially.
   */
  glwindow = create_glwindow ();
  gtk_widget_show (glwindow);
  fileselection = create_fileselection ();
  gtk_widget_show (fileselection);
  pakdialog = create_pakdialog ();
  gtk_widget_show (pakdialog);
  dialog = create_dialog ();
  gtk_widget_show (dialog);

  gtk_main ();
  return 0;
}

void
on_glwindow_destroy                    (GtkObject       *object,
                                        gpointer         user_data)
{

}

gboolean
on_fileselection_delete                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{

  return FALSE;
}

void
on_ok_clicked                          (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_cancel_clicked                      (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_dialog_destroy                      (GtkObject       *object,
                                        gpointer         user_data)
{

}

void
on_load_clicked                        (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_images_menu_enter                   (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_scale_clicked                       (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_base_texture_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_center_clicked                      (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_update_clicked                      (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_button_toggled                      (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

}

void
on_paint_clicked                       (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_generate_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{

}

void
on_button_toggled                      (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

}

void
on_button_toggled                      (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

}

void
on_button_toggled                      (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

}

void
on_button_toggled                      (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

}

void
on_close_clicked                       (GtkButton       *button,
                                        gpointer         user_data)
{

}

