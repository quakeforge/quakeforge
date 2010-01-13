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

/*
 * This function returns a widget in a component created by Glade.
 * Call it with the toplevel widget in the component (i.e. a window/dialog),
 * or alternatively any widget in the component, and the name of the widget
 * you want returned.
 */
GtkWidget*
get_widget                             (GtkWidget       *widget,
                                        gchar           *widget_name);


 /*
  * This is an internally used function for setting notebook tabs. It is
  * included in this header file only so you don't get compilation warnings
  */
void
set_notebook_tab                       (GtkWidget       *notebook,
                                        gint             page_num,
                                        GtkWidget       *widget);

GtkWidget* create_glwindow (void);
GtkWidget* create_fileselection (void);
GtkWidget* create_pakdialog (void);
GtkWidget* create_dialog (void);
