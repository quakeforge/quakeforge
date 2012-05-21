/* GiMd2Viewer - Quake2 model viewer
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "pack.h"

#define INFO_KEY "Entry Info"

typedef struct {
  unsigned char magic[4];
  int diroffset;
  int dirsize;
} PakHeader;

typedef struct {
  char filename[0x38];
  int offset;
  int size;
} PakEntry;

typedef struct dir_entry {
  char *name;
  int offset;
  int size;

  struct dir_entry *same_level;
  struct dir_entry *sub_dir;
} DirEntry;


static GtkSignalFunc signal_func;
static gpointer signal_data;

void SetItemParams(GtkSignalFunc f,gpointer data)
{
	signal_func=f;
	signal_data=data;
}

char *GetDir(char **file) {
  char *tmp = *file;
  int i;

  for (i = 0; (tmp[i] != '/') && (tmp[i] != '\0'); i++) ;

  if (tmp[i] == '\0')
    return NULL;

  tmp[i] = '\0';
  *file = *file + i + 1;

  return tmp;
}

static inline DirEntry *NewDirEntry(char *name, DirEntry *level, DirEntry *sub,
				    int offset, int size) {
  DirEntry *ret = (DirEntry *) malloc(sizeof(DirEntry));

  ret->name = name;
  ret->same_level = level;
  ret->sub_dir = sub;

  ret->offset = offset;
  ret->size = size;

  return ret;
}

static inline DirEntry *RecInsert(DirEntry *first, char *filename,
				  int offset, int size) {
  char *dir, *subdir;

  subdir = filename;
  dir = GetDir(&subdir);

  if (dir == NULL) {
    /* This is a leaf (a file) */
    DirEntry *prec, *cur, *newentry;

    newentry = NewDirEntry(subdir, NULL, NULL, offset, size);

    prec = NULL;
    cur = first;

    while (cur != NULL) {
      if (strcmp(subdir, cur->name) <= 0)
	break;

      prec = cur;
      cur = cur->same_level;
    }

    if (prec == NULL) {
      newentry->same_level = first;

      return newentry;
    } else {
      prec->same_level = newentry;
      newentry->same_level = cur;

      return first;
    }
  } else {
    /* The file is in a directory */
    DirEntry *prec, *cur, *newentry;

    prec = NULL;
    cur = first;

    while (cur != NULL) {
      if (strcmp(dir, cur->name) <= 0)
	break;

      prec = cur;
      cur = cur->same_level;
    }

    /* Already created subdirectory */
    if ((cur != NULL) && (strcmp(dir, cur->name) == 0)) {
      cur->sub_dir = RecInsert(cur->sub_dir, subdir, offset, size);

      return first;
    }

    /* Case of a new directory */
    newentry = NewDirEntry(dir, NULL, NULL, -1, -1);
    newentry->sub_dir = RecInsert(NULL, subdir, offset, size);

    if (prec == NULL) {
      newentry->same_level = first;

      return newentry;
    } else {
      prec->same_level = newentry;
      newentry->same_level = cur;

      return first;
    }
  }
}

static void item_destroyed(GtkWidget *widget, gpointer data) {
  EntryInfo *info = (EntryInfo *) gtk_object_get_data(GTK_OBJECT(widget),
						      INFO_KEY);

  if (info == NULL)
    return;

  free(info);
  gtk_object_set_data(GTK_OBJECT(widget),
		      INFO_KEY,
		      NULL);
}

static inline void BuildTree(GtkWidget *tree, DirEntry *de) {
  while (de != NULL) {
    GtkWidget *item;

    /* Creates a new tree item */
    item = gtk_tree_item_new_with_label(de->name);
    gtk_tree_append(GTK_TREE(tree), item);
    gtk_widget_show(item);

    if (de->sub_dir == NULL) {
      /* This is a file !!!! */
      EntryInfo *info = (EntryInfo *) malloc(sizeof(EntryInfo));
      info->offset = de->offset;
      info->size = de->size;

      gtk_object_set_data(GTK_OBJECT(item),
			  INFO_KEY,
			  (gpointer) info);

	if (signal_func)
	{
		gtk_widget_set_events(item,
			GDK_BUTTON_PRESS_MASK|gtk_widget_get_events(item));

		gtk_signal_connect(GTK_OBJECT(item),"button_press_event",
			signal_func,signal_data);
	}

      /* To free the memory */
      gtk_signal_connect(GTK_OBJECT(item), "destroy",
			 GTK_SIGNAL_FUNC(item_destroyed), NULL);
    } else {
      GtkWidget *stree;

      stree = gtk_tree_new();
      gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), stree);
      gtk_tree_item_collapse(GTK_TREE_ITEM(item));
      BuildTree(stree, de->sub_dir);
    }

    de = de->same_level;
  }
}

static inline void FreeMem(DirEntry *de) {
  if (de == NULL)
    return;
  FreeMem(de->sub_dir);
  FreeMem(de->same_level);

  /* No need to free the char *, it is freed with the PakEntries */
  free(de);
}

EntryInfo *GetInfo(GtkWidget *item) {
  return (EntryInfo *) gtk_object_get_data(GTK_OBJECT(item),
					   INFO_KEY);
}

GtkWidget *OpenPAK(FILE *f, char *name) {
  PakHeader header;
  PakEntry *entries;
  int num_entries;
  int i;
  DirEntry *first = NULL, *root;
  GtkWidget *ret;

  /* Fail safe :-) */
  if (f == NULL)
    return NULL;

  /* Loads the header */
  fread(&header, sizeof(PakHeader), 1, f);

  /* Check if this is really a PACK file */
  if (strncmp((char *) &(header.magic), "PACK", 4))
    return NULL;

  /* Goes to the start of the PAK directory */
  fseek(f, header.diroffset, SEEK_SET);
  num_entries = header.dirsize / sizeof(PakEntry);

  /* Reads the PAK directory */
  entries = (PakEntry *) malloc(num_entries * sizeof(PakEntry));
  fread(entries, header.dirsize, 1, f);

  /* Builds the dir hierarchy */
  for (i = 0; i < num_entries; i++) {
    first = RecInsert(first, entries[i].filename,
		      entries[i].offset, entries[i].size);
  }
  root = NewDirEntry(name, NULL, first, -1, -1);

  /* Builds the corresponding GtkTree */
  ret = gtk_tree_new();
  BuildTree(ret, root);

  /* Frees the memory */
  FreeMem(root);
  free(entries);

  return ret;
}
