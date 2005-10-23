/* GiMd2Viewer - Quake2 model viewer
 * Copyright (C) 1998  Lionel ULMER <bbrox@mygale.org>
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

#ifndef __PACK_H__

#include <stdio.h>

typedef struct entry_info {
  int offset;
  int size;
} EntryInfo;

EntryInfo *GetInfo(GtkWidget *item) ;
GtkWidget *OpenPAK(FILE *f, char *name);
void SetItemParams(GtkSignalFunc func, gpointer data);

#endif	// __PACK_H__
