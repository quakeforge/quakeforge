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
#include <gdk/gdkkeysyms.h>
#include "gladesig.h"
#include "gladesrc.h"

GtkWidget*
get_widget                             (GtkWidget       *widget,
                                        gchar           *widget_name)
{
  GtkWidget *found_widget;

  if (widget->parent)
    widget = gtk_widget_get_toplevel (widget);
  found_widget = (GtkWidget*) gtk_object_get_data (GTK_OBJECT (widget),
                                                   widget_name);
  if (!found_widget)
    g_warning ("Widget not found: %s", widget_name);
  return found_widget;
}

/* This is an internally used function to set notebook tab widgets. */
void
set_notebook_tab                       (GtkWidget       *notebook,
                                        gint             page_num,
                                        GtkWidget       *widget)
{
  GtkNotebookPage *page;
  GtkWidget *notebook_page;

  page = (GtkNotebookPage*) g_list_nth (GTK_NOTEBOOK (notebook)->children, page_num)->data;
  notebook_page = page->child;
  gtk_widget_ref (notebook_page);
  gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), page_num);
  gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), notebook_page,
                            widget, page_num);
  gtk_widget_unref (notebook_page);
}

GtkWidget*
create_glwindow ()
{
  GtkWidget *glwindow;

  glwindow = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_object_set_data (GTK_OBJECT (glwindow), "glwindow", glwindow);
  gtk_signal_connect (GTK_OBJECT (glwindow), "destroy",
                      GTK_SIGNAL_FUNC (on_glwindow_destroy),
                      NULL);
  gtk_window_set_title (GTK_WINDOW (glwindow), "3D Window");
  gtk_window_set_policy (GTK_WINDOW (glwindow), TRUE, TRUE, FALSE);

  return glwindow;
}

GtkWidget*
create_fileselection ()
{
  GtkWidget *fileselection;
  GtkWidget *ok_button1;
  GtkWidget *cancel_button1;

  fileselection = gtk_file_selection_new ("Select a PAK, MDL or MD2 file");
  gtk_object_set_data (GTK_OBJECT (fileselection), "fileselection", fileselection);
  gtk_container_border_width (GTK_CONTAINER (fileselection), 10);
  gtk_signal_connect (GTK_OBJECT (fileselection), "delete_event",
                      GTK_SIGNAL_FUNC (on_fileselection_delete),
                      NULL);

  ok_button1 = GTK_FILE_SELECTION (fileselection)->ok_button;
  gtk_object_set_data (GTK_OBJECT (fileselection), "ok_button1", ok_button1);
  gtk_widget_show (ok_button1);
  GTK_WIDGET_SET_FLAGS (ok_button1, GTK_CAN_DEFAULT);
  gtk_signal_connect (GTK_OBJECT (ok_button1), "clicked",
                      GTK_SIGNAL_FUNC (on_ok_clicked),
                      NULL);

  cancel_button1 = GTK_FILE_SELECTION (fileselection)->cancel_button;
  gtk_object_set_data (GTK_OBJECT (fileselection), "cancel_button1", cancel_button1);
  gtk_widget_show (cancel_button1);
  GTK_WIDGET_SET_FLAGS (cancel_button1, GTK_CAN_DEFAULT);
  gtk_signal_connect (GTK_OBJECT (cancel_button1), "clicked",
                      GTK_SIGNAL_FUNC (on_cancel_clicked),
                      NULL);

  return fileselection;
}

GtkWidget*
create_pakdialog ()
{
  GtkWidget *pakdialog;
  GtkWidget *vbox8;
  GtkWidget *filename;
  GtkWidget *scrolledwindow;
  GtkWidget *open;
  GtkWidget *close;

  pakdialog = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_object_set_data (GTK_OBJECT (pakdialog), "pakdialog", pakdialog);
  gtk_widget_set_usize (pakdialog, 256, 256);
  gtk_container_border_width (GTK_CONTAINER (pakdialog), 4);
  gtk_window_set_title (GTK_WINDOW (pakdialog), "Select a MDL, MD2 or PCX file");
  gtk_window_set_policy (GTK_WINDOW (pakdialog), TRUE, TRUE, FALSE);

  vbox8 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (pakdialog), "vbox8", vbox8);
  gtk_widget_show (vbox8);
  gtk_container_add (GTK_CONTAINER (pakdialog), vbox8);

  filename = gtk_label_new ("filename");
  gtk_object_set_data (GTK_OBJECT (pakdialog), "filename", filename);
  gtk_widget_show (filename);
  gtk_box_pack_start (GTK_BOX (vbox8), filename, FALSE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (filename), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (filename), 0, 0.5);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_object_set_data (GTK_OBJECT (pakdialog), "scrolledwindow", scrolledwindow);
  gtk_widget_show (scrolledwindow);
  gtk_box_pack_start (GTK_BOX (vbox8), scrolledwindow, TRUE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (scrolledwindow), 4);

  open = gtk_button_new_with_label ("Open");
  gtk_object_set_data (GTK_OBJECT (pakdialog), "open", open);
  gtk_widget_show (open);
  gtk_box_pack_start (GTK_BOX (vbox8), open, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (open), 4);

  close = gtk_button_new_with_label ("Close");
  gtk_object_set_data (GTK_OBJECT (pakdialog), "close", close);
  gtk_widget_show (close);
  gtk_box_pack_start (GTK_BOX (vbox8), close, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (close), 4);

  return pakdialog;
}

GtkWidget*
create_dialog ()
{
  GtkWidget *dialog;
  GtkWidget *dialog_vbox2;
  GtkWidget *hbox7;
  GtkWidget *label14;
  GtkWidget *models_menu;
  GtkWidget *models_menu_menu;
  GtkWidget *glade_menuitem;
  GtkWidget *notebook1;
  GtkWidget *vbox15;
  GtkWidget *button19;
  GtkWidget *frame11;
  GtkWidget *vbox17;
  GtkWidget *hbox11;
  GtkWidget *label30;
  GtkWidget *images_menu;
  GtkWidget *images_menu_menu;
  GtkWidget *info;
  GtkWidget *table4;
  GtkObject *height_adj;
  GtkWidget *height;
  GtkObject *width_adj;
  GtkWidget *width;
  GtkWidget *l30;
  GtkWidget *l20;
  GtkWidget *new_image;
  GtkWidget *scale;
  GtkWidget *base_texture;
  GtkWidget *vbox16;
  GtkWidget *button21;
  GtkWidget *frame12;
  GtkWidget *vbox18;
  GtkWidget *button26;
  GtkWidget *hbox13;
  GtkWidget *update;
  GtkWidget *update_time;
  GtkWidget *label34;
  GtkWidget *frame13;
  GtkWidget *hbox14;
  GtkWidget *button27;
  GtkWidget *button28;
  GtkWidget *vbox9;
  GtkWidget *vbox11;
  GtkWidget *frame9;
  GtkWidget *vbox12;
  GSList *perspective_group = NULL;
  GtkWidget *fastest;
  GtkWidget *nicest;
  GtkWidget *frame10;
  GtkWidget *vbox13;
  GSList *filter_group = NULL;
  GtkWidget *nearest;
  GtkWidget *linear;
  GtkWidget *vbox14;
  GtkWidget *frame_info;
  GtkWidget *frame_info2;
  GtkWidget *table1;
  GtkWidget *fps;
  GtkWidget *l4;
  GtkWidget *l1;
  GtkWidget *l3;
  GtkWidget *l2;
  GtkWidget *end_frame;
  GtkWidget *start_frame;
  GtkWidget *cur_frame;
  GtkWidget *anim_hbox;
  GtkWidget *vbox25;
  GtkWidget *frame17;
  GtkWidget *vbox26;
  GtkWidget *version;
  GtkWidget *label779;
  GtkWidget *label780;
  GtkWidget *label7;
  GtkWidget *label8;
  GtkWidget *label9;
  GtkWidget *label10;
  GtkWidget *label777;
  GtkWidget *dialog_action_area2;
  GtkWidget *close;

  dialog = gtk_dialog_new ();
  gtk_object_set_data (GTK_OBJECT (dialog), "dialog", dialog);
  gtk_container_border_width (GTK_CONTAINER (dialog), 2);
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
                      GTK_SIGNAL_FUNC (on_dialog_destroy),
                      NULL);
  gtk_window_set_title (GTK_WINDOW (dialog), "Texture Paint");
  gtk_window_set_policy (GTK_WINDOW (dialog), TRUE, TRUE, FALSE);

  dialog_vbox2 = GTK_DIALOG (dialog)->vbox;
  gtk_object_set_data (GTK_OBJECT (dialog), "dialog_vbox2", dialog_vbox2);
  gtk_widget_show (dialog_vbox2);

  hbox7 = gtk_hbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "hbox7", hbox7);
  gtk_widget_show (hbox7);
  gtk_box_pack_start (GTK_BOX (dialog_vbox2), hbox7, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (hbox7), 4);

  label14 = gtk_label_new ("Model:");
  gtk_object_set_data (GTK_OBJECT (dialog), "label14", label14);
  gtk_widget_show (label14);
  gtk_box_pack_start (GTK_BOX (hbox7), label14, FALSE, TRUE, 0);

  models_menu = gtk_option_menu_new ();
  gtk_object_set_data (GTK_OBJECT (dialog), "models_menu", models_menu);
  gtk_widget_show (models_menu);
  gtk_box_pack_start (GTK_BOX (hbox7), models_menu, TRUE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (models_menu), 4);
  models_menu_menu = gtk_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (models_menu), models_menu_menu);

  notebook1 = gtk_notebook_new ();
  gtk_object_set_data (GTK_OBJECT (dialog), "notebook1", notebook1);
  gtk_widget_show (notebook1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox2), notebook1, TRUE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (notebook1), 4);

  vbox15 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox15", vbox15);
  gtk_widget_show (vbox15);
  gtk_container_add (GTK_CONTAINER (notebook1), vbox15);

  button19 = gtk_button_new_with_label ("Load Model / PAK / Image");
  gtk_object_set_data (GTK_OBJECT (dialog), "button19", button19);
  gtk_widget_show (button19);
  gtk_box_pack_start (GTK_BOX (vbox15), button19, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (button19), 4);
  gtk_signal_connect (GTK_OBJECT (button19), "clicked",
                      GTK_SIGNAL_FUNC (on_load_clicked),
                      NULL);

  frame11 = gtk_frame_new ("Texture");
  gtk_object_set_data (GTK_OBJECT (dialog), "frame11", frame11);
  gtk_widget_show (frame11);
  gtk_box_pack_start (GTK_BOX (vbox15), frame11, TRUE, TRUE, 0);

  vbox17 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox17", vbox17);
  gtk_widget_show (vbox17);
  gtk_container_add (GTK_CONTAINER (frame11), vbox17);

  hbox11 = gtk_hbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "hbox11", hbox11);
  gtk_widget_show (hbox11);
  gtk_box_pack_start (GTK_BOX (vbox17), hbox11, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (hbox11), 4);

  label30 = gtk_label_new ("Image: ");
  gtk_object_set_data (GTK_OBJECT (dialog), "label30", label30);
  gtk_widget_show (label30);
  gtk_box_pack_start (GTK_BOX (hbox11), label30, FALSE, TRUE, 0);

  images_menu = gtk_option_menu_new ();
  gtk_object_set_data (GTK_OBJECT (dialog), "images_menu", images_menu);
  gtk_widget_show (images_menu);
  gtk_box_pack_start (GTK_BOX (hbox11), images_menu, TRUE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (images_menu), 2);
  gtk_signal_connect (GTK_OBJECT (images_menu), "enter",
                      GTK_SIGNAL_FUNC (on_images_menu_enter),
                      NULL);
  images_menu_menu = gtk_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (images_menu), images_menu_menu);

  info = gtk_label_new ("info");
  gtk_object_set_data (GTK_OBJECT (dialog), "info", info);
  gtk_widget_show (info);
  gtk_box_pack_start (GTK_BOX (vbox17), info, FALSE, TRUE, 0);

  table4 = gtk_table_new (2, 2, FALSE);
  gtk_object_set_data (GTK_OBJECT (dialog), "table4", table4);
  gtk_widget_show (table4);
  gtk_box_pack_start (GTK_BOX (vbox17), table4, FALSE, TRUE, 0);

  height_adj = gtk_adjustment_new (1, 1, 5000, 1, 10, 10);
  height = gtk_spin_button_new (GTK_ADJUSTMENT (height_adj), 1, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "height", height);
  gtk_widget_show (height);
  gtk_table_attach (GTK_TABLE (table4), height, 1, 2, 1, 2,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  width_adj = gtk_adjustment_new (1, 1, 5000, 1, 10, 10);
  width = gtk_spin_button_new (GTK_ADJUSTMENT (width_adj), 1, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "width", width);
  gtk_widget_show (width);
  gtk_table_attach (GTK_TABLE (table4), width, 1, 2, 0, 1,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  l30 = gtk_label_new ("New Height:");
  gtk_object_set_data (GTK_OBJECT (dialog), "l30", l30);
  gtk_widget_show (l30);
  gtk_table_attach (GTK_TABLE (table4), l30, 0, 1, 1, 2,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l30), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (l30), 0, 0.5);

  l20 = gtk_label_new ("New Width:");
  gtk_object_set_data (GTK_OBJECT (dialog), "l20", l20);
  gtk_widget_show (l20);
  gtk_table_attach (GTK_TABLE (table4), l20, 0, 1, 0, 1,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l20), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (l20), 0, 0.5);

  new_image = gtk_check_button_new_with_label ("Create new Image");
  gtk_object_set_data (GTK_OBJECT (dialog), "new_image", new_image);
  gtk_widget_show (new_image);
  gtk_box_pack_start (GTK_BOX (vbox17), new_image, FALSE, TRUE, 0);

  scale = gtk_button_new_with_label ("Scale / Convert RGB");
  gtk_object_set_data (GTK_OBJECT (dialog), "scale", scale);
  gtk_widget_show (scale);
  gtk_box_pack_start (GTK_BOX (vbox17), scale, FALSE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (scale), "clicked",
                      GTK_SIGNAL_FUNC (on_scale_clicked),
                      NULL);

  base_texture = gtk_button_new_with_label ("Base Texture");
  gtk_object_set_data (GTK_OBJECT (dialog), "base_texture", base_texture);
  gtk_widget_show (base_texture);
  gtk_box_pack_start (GTK_BOX (vbox17), base_texture, FALSE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (base_texture), "clicked",
                      GTK_SIGNAL_FUNC (on_base_texture_clicked),
                      NULL);

  vbox16 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox16", vbox16);
  gtk_widget_show (vbox16);
  gtk_container_add (GTK_CONTAINER (notebook1), vbox16);
  gtk_container_border_width (GTK_CONTAINER (vbox16), 4);

  button21 = gtk_button_new_with_label ("Center Model");
  gtk_object_set_data (GTK_OBJECT (dialog), "button21", button21);
  gtk_widget_show (button21);
  gtk_box_pack_start (GTK_BOX (vbox16), button21, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (button21), 4);
  gtk_signal_connect (GTK_OBJECT (button21), "clicked",
                      GTK_SIGNAL_FUNC (on_center_clicked),
                      NULL);

  frame12 = gtk_frame_new ("Texture Paint");
  gtk_object_set_data (GTK_OBJECT (dialog), "frame12", frame12);
  gtk_widget_show (frame12);
  gtk_box_pack_start (GTK_BOX (vbox16), frame12, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (frame12), 4);

  vbox18 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox18", vbox18);
  gtk_widget_show (vbox18);
  gtk_container_add (GTK_CONTAINER (frame12), vbox18);

  button26 = gtk_button_new_with_label ("Update");
  gtk_object_set_data (GTK_OBJECT (dialog), "button26", button26);
  gtk_widget_show (button26);
  gtk_box_pack_start (GTK_BOX (vbox18), button26, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (button26), 4);
  gtk_signal_connect (GTK_OBJECT (button26), "clicked",
                      GTK_SIGNAL_FUNC (on_update_clicked),
                      NULL);

  hbox13 = gtk_hbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "hbox13", hbox13);
  gtk_widget_show (hbox13);
  gtk_box_pack_start (GTK_BOX (vbox18), hbox13, FALSE, TRUE, 0);

  update = gtk_check_button_new_with_label ("Update after");
  gtk_object_set_data (GTK_OBJECT (dialog), "update", update);
  gtk_widget_show (update);
  gtk_box_pack_start (GTK_BOX (hbox13), update, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (update), 4);
  gtk_signal_connect (GTK_OBJECT (update), "toggled",
                      GTK_SIGNAL_FUNC (on_button_toggled),
                      NULL);

  update_time = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (0.25, 0, 5, 0, 0, 0)));
  gtk_object_set_data (GTK_OBJECT (dialog), "update_time", update_time);
  gtk_widget_show (update_time);
  gtk_box_pack_start (GTK_BOX (hbox13), update_time, TRUE, TRUE, 0);
  gtk_scale_set_value_pos (GTK_SCALE (update_time), GTK_POS_RIGHT);
  gtk_scale_set_digits (GTK_SCALE (update_time), 2);

  label34 = gtk_label_new (" seconds.");
  gtk_object_set_data (GTK_OBJECT (dialog), "label34", label34);
  gtk_widget_show (label34);
  gtk_box_pack_start (GTK_BOX (hbox13), label34, FALSE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (label34), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label34), 0, 0.5);

  frame13 = gtk_frame_new ("3D Paint");
  gtk_object_set_data (GTK_OBJECT (dialog), "frame13", frame13);
  gtk_widget_show (frame13);
  gtk_box_pack_start (GTK_BOX (vbox16), frame13, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (frame13), 4);

  hbox14 = gtk_hbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "hbox14", hbox14);
  gtk_widget_show (hbox14);
  gtk_container_add (GTK_CONTAINER (frame13), hbox14);
  gtk_container_border_width (GTK_CONTAINER (hbox14), 4);

  button27 = gtk_button_new_with_label ("Open 3D Paint Window");
  gtk_object_set_data (GTK_OBJECT (dialog), "button27", button27);
  gtk_widget_show (button27);
  gtk_box_pack_start (GTK_BOX (hbox14), button27, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (button27), "clicked",
                      GTK_SIGNAL_FUNC (on_paint_clicked),
                      NULL);

  button28 = gtk_button_new_with_label ("Calculate Texture");
  gtk_object_set_data (GTK_OBJECT (dialog), "button28", button28);
  gtk_widget_show (button28);
  gtk_box_pack_start (GTK_BOX (hbox14), button28, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (button28), "clicked",
                      GTK_SIGNAL_FUNC (on_generate_clicked),
                      NULL);

  vbox9 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox9", vbox9);
  gtk_widget_show (vbox9);
  gtk_container_add (GTK_CONTAINER (notebook1), vbox9);

  vbox11 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox11", vbox11);
  gtk_widget_show (vbox11);
  gtk_box_pack_start (GTK_BOX (vbox9), vbox11, TRUE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (vbox11), 4);

  frame9 = gtk_frame_new ("Perspective correction");
  gtk_object_set_data (GTK_OBJECT (dialog), "frame9", frame9);
  gtk_widget_show (frame9);
  gtk_box_pack_start (GTK_BOX (vbox11), frame9, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (frame9), 4);

  vbox12 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox12", vbox12);
  gtk_widget_show (vbox12);
  gtk_container_add (GTK_CONTAINER (frame9), vbox12);

  fastest = gtk_radio_button_new_with_label (perspective_group, "fastest");
  perspective_group = gtk_radio_button_group (GTK_RADIO_BUTTON (fastest));
  gtk_object_set_data (GTK_OBJECT (dialog), "fastest", fastest);
  gtk_widget_show (fastest);
  gtk_box_pack_start (GTK_BOX (vbox12), fastest, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (fastest), "toggled",
                      GTK_SIGNAL_FUNC (on_button_toggled),
                      NULL);

  nicest = gtk_radio_button_new_with_label (perspective_group, "nicest");
  perspective_group = gtk_radio_button_group (GTK_RADIO_BUTTON (nicest));
  gtk_object_set_data (GTK_OBJECT (dialog), "nicest", nicest);
  gtk_widget_show (nicest);
  gtk_box_pack_start (GTK_BOX (vbox12), nicest, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (nicest), "toggled",
                      GTK_SIGNAL_FUNC (on_button_toggled),
                      NULL);

  frame10 = gtk_frame_new ("Texture filter");
  gtk_object_set_data (GTK_OBJECT (dialog), "frame10", frame10);
  gtk_widget_show (frame10);
  gtk_box_pack_start (GTK_BOX (vbox11), frame10, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (frame10), 4);

  vbox13 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox13", vbox13);
  gtk_widget_show (vbox13);
  gtk_container_add (GTK_CONTAINER (frame10), vbox13);

  nearest = gtk_radio_button_new_with_label (filter_group, "nearest");
  filter_group = gtk_radio_button_group (GTK_RADIO_BUTTON (nearest));
  gtk_object_set_data (GTK_OBJECT (dialog), "nearest", nearest);
  gtk_widget_show (nearest);
  gtk_box_pack_start (GTK_BOX (vbox13), nearest, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (nearest), "toggled",
                      GTK_SIGNAL_FUNC (on_button_toggled),
                      NULL);

  linear = gtk_radio_button_new_with_label (filter_group, "linear");
  filter_group = gtk_radio_button_group (GTK_RADIO_BUTTON (linear));
  gtk_object_set_data (GTK_OBJECT (dialog), "linear", linear);
  gtk_widget_show (linear);
  gtk_box_pack_start (GTK_BOX (vbox13), linear, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (linear), "toggled",
                      GTK_SIGNAL_FUNC (on_button_toggled),
                      NULL);

  vbox14 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox14", vbox14);
  gtk_widget_show (vbox14);
  gtk_container_add (GTK_CONTAINER (notebook1), vbox14);
  gtk_container_border_width (GTK_CONTAINER (vbox14), 4);

  frame_info = gtk_label_new ("Info");
  gtk_object_set_data (GTK_OBJECT (dialog), "frame_info", frame_info);
  gtk_widget_show (frame_info);
  gtk_box_pack_start (GTK_BOX (vbox14), frame_info, FALSE, TRUE, 0);

  frame_info2 = gtk_label_new ("frame 123");
  gtk_object_set_data (GTK_OBJECT (dialog), "frame_info2", frame_info2);
  gtk_widget_show (frame_info2);
  gtk_box_pack_start (GTK_BOX (vbox14), frame_info2, FALSE, TRUE, 0);

  table1 = gtk_table_new (4, 2, FALSE);
  gtk_object_set_data (GTK_OBJECT (dialog), "table1", table1);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox14), table1, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (table1), 4);

  fps = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (18, 0, 50, 1, 4, 0)));
  gtk_object_set_data (GTK_OBJECT (dialog), "fps", fps);
  gtk_widget_show (fps);
  gtk_table_attach (GTK_TABLE (table1), fps, 1, 2, 3, 4,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  l4 = gtk_label_new ("Frames per second  ");
  gtk_object_set_data (GTK_OBJECT (dialog), "l4", l4);
  gtk_widget_show (l4);
  gtk_table_attach (GTK_TABLE (table1), l4, 0, 1, 3, 4,
                    GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l4), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (l4), 0, 1);

  l1 = gtk_label_new ("Current Frame  ");
  gtk_object_set_data (GTK_OBJECT (dialog), "l1", l1);
  gtk_widget_show (l1);
  gtk_table_attach (GTK_TABLE (table1), l1, 0, 1, 0, 1,
                    GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l1), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (l1), 0, 1);

  l3 = gtk_label_new ("End Frame");
  gtk_object_set_data (GTK_OBJECT (dialog), "l3", l3);
  gtk_widget_show (l3);
  gtk_table_attach (GTK_TABLE (table1), l3, 0, 1, 2, 3,
                    GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l3), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (l3), 0, 1);

  l2 = gtk_label_new ("Start Frame");
  gtk_object_set_data (GTK_OBJECT (dialog), "l2", l2);
  gtk_widget_show (l2);
  gtk_table_attach (GTK_TABLE (table1), l2, 0, 1, 1, 2,
                    GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_label_set_justify (GTK_LABEL (l2), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (l2), 0, 1);

  end_frame = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 0, 1, 10, 0)));
  gtk_object_set_data (GTK_OBJECT (dialog), "end_frame", end_frame);
  gtk_widget_show (end_frame);
  gtk_table_attach (GTK_TABLE (table1), end_frame, 1, 2, 2, 3,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_scale_set_digits (GTK_SCALE (end_frame), 0);

  start_frame = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 0, 1, 10, 0)));
  gtk_object_set_data (GTK_OBJECT (dialog), "start_frame", start_frame);
  gtk_widget_show (start_frame);
  gtk_table_attach (GTK_TABLE (table1), start_frame, 1, 2, 1, 2,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_scale_set_digits (GTK_SCALE (start_frame), 0);

  cur_frame = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 0, 1, 10, 0)));
  gtk_object_set_data (GTK_OBJECT (dialog), "cur_frame", cur_frame);
  gtk_widget_show (cur_frame);
  gtk_table_attach (GTK_TABLE (table1), cur_frame, 1, 2, 0, 1,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_scale_set_digits (GTK_SCALE (cur_frame), 0);

  anim_hbox = gtk_hbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "anim_hbox", anim_hbox);
  gtk_widget_show (anim_hbox);
  gtk_box_pack_start (GTK_BOX (vbox14), anim_hbox, FALSE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (anim_hbox), 10);

  vbox25 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox25", vbox25);
  gtk_widget_show (vbox25);
  gtk_container_add (GTK_CONTAINER (notebook1), vbox25);

  frame17 = gtk_frame_new ("");
  gtk_object_set_data (GTK_OBJECT (dialog), "frame17", frame17);
  gtk_widget_show (frame17);
  gtk_box_pack_start (GTK_BOX (vbox25), frame17, TRUE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (frame17), 2);
  gtk_frame_set_label_align (GTK_FRAME (frame17), 0.5, 0.5);

  vbox26 = gtk_vbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (dialog), "vbox26", vbox26);
  gtk_widget_show (vbox26);
  gtk_container_add (GTK_CONTAINER (frame17), vbox26);
  gtk_container_border_width (GTK_CONTAINER (vbox26), 2);

  version = gtk_label_new ("Texture Paint ???");
  gtk_object_set_data (GTK_OBJECT (dialog), "version", version);
  gtk_widget_show (version);
  gtk_box_pack_start (GTK_BOX (vbox26), version, TRUE, TRUE, 0);

  label779 = gtk_label_new ("Uwe Maurer <uwe_maurer@t-online.de>");
  gtk_object_set_data (GTK_OBJECT (dialog), "label779", label779);
  gtk_widget_show (label779);
  gtk_box_pack_start (GTK_BOX (vbox26), label779, TRUE, TRUE, 0);

  label780 = gtk_label_new ("http://home.t-online.de/home/uwe_maurer/texpaint.htm");
  gtk_object_set_data (GTK_OBJECT (dialog), "label780", label780);
  gtk_widget_show (label780);
  gtk_box_pack_start (GTK_BOX (vbox26), label780, TRUE, TRUE, 0);

  label7 = gtk_label_new ("File");
  gtk_object_set_data (GTK_OBJECT (dialog), "label7", label7);
  gtk_widget_show (label7);
  set_notebook_tab (notebook1, 0, label7);

  label8 = gtk_label_new ("Paint");
  gtk_object_set_data (GTK_OBJECT (dialog), "label8", label8);
  gtk_widget_show (label8);
  set_notebook_tab (notebook1, 1, label8);

  label9 = gtk_label_new ("Options");
  gtk_object_set_data (GTK_OBJECT (dialog), "label9", label9);
  gtk_widget_show (label9);
  set_notebook_tab (notebook1, 2, label9);

  label10 = gtk_label_new ("Animation");
  gtk_object_set_data (GTK_OBJECT (dialog), "label10", label10);
  gtk_widget_show (label10);
  set_notebook_tab (notebook1, 3, label10);

  label777 = gtk_label_new ("About");
  gtk_object_set_data (GTK_OBJECT (dialog), "label777", label777);
  gtk_widget_show (label777);
  set_notebook_tab (notebook1, 4, label777);

  dialog_action_area2 = GTK_DIALOG (dialog)->action_area;
  gtk_object_set_data (GTK_OBJECT (dialog), "dialog_action_area2", dialog_action_area2);
  gtk_widget_show (dialog_action_area2);
  gtk_container_border_width (GTK_CONTAINER (dialog_action_area2), 10);

  close = gtk_button_new_with_label ("Close");
  gtk_object_set_data (GTK_OBJECT (dialog), "close", close);
  gtk_widget_show (close);
  gtk_box_pack_start (GTK_BOX (dialog_action_area2), close, TRUE, TRUE, 0);
  gtk_signal_connect (GTK_OBJECT (close), "clicked",
                      GTK_SIGNAL_FUNC (on_close_clicked),
                      NULL);

  return dialog;
}

