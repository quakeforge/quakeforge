/*
	riff.c

	riff wav handling

	Copyright (C) 2003 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2003/4/10

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/qendian.h"
#include "QF/riff.h"

static inline int
Rread (QFile *f, void *buf, int len)
{
	int         count;

	count = Qread (f, buf, len);
	if (count != len)
		return 0;
	return count;
}

static char *
read_string (QFile *f, int len)
{
	char        c[2] = {0, 0};
	char       *s;
	int         l = len;
	dstring_t  *str;

	if (!len)
		return 0;
	str = dstring_newstr ();
	while (l--) {
		if (Qread (f, c, 1) != 1)
			goto done;
		if (!c[0])
			break;
		dstring_appendstr (str, c);
	}
	Qseek (f, l, SEEK_CUR);
	if (len &1) {
		int         c;

		if ((c = Qgetc (f)) && c != -1)
			Qungetc (f, c);
	}
done:
	s = str->str;
	free (str);
	return s;
}

static void *
read_data (QFile *f, int len)
{
	void       *data;
	int         count;

	if (!len)
		return 0;
	data = malloc (len);
	count = Qread (f, data, len);
	if (count == len && len &1) {
		int         c;

		if ((c = Qgetc (f)) && c != -1)
			Qungetc (f, c);
	}
	if (count && count != len)
		data = realloc (data, count);
	if (!count) {
		free (data);
		return 0;
	}
	return data;
}

static int
read_ltxt (QFile *f, int len, riff_d_ltxt_t *ltxt)
{
	if (!Rread (f, ltxt, len))
		return 0;
	ltxt->name = LittleLong (ltxt->name);
	ltxt->len = LittleLong (ltxt->len);
	ltxt->country = LittleLong (ltxt->country);
	ltxt->language = LittleLong (ltxt->language);
	ltxt->dialect = LittleLong (ltxt->dialect);
	ltxt->codepage = LittleLong (ltxt->codepage);
	return 1;
}

static void
read_adtl (dstring_t *list_buf, QFile *f, int len)
{
	riff_d_chunk_t  ck;
	riff_d_chunk_t  *chunk = 0;
	riff_ltxt_t     *ltxt;
	riff_label_t    *label;
	riff_data_t     *data;
	//FIXME riff_list_t     *list;

	//FIXME list = (riff_list_t *) list_buf->str;
	while (len) {
		if (!Rread (f, &ck, sizeof (ck))) {
			break;
		}
		len -= sizeof (ck);
		ck.len = LittleLong (ck.len);
		RIFF_SWITCH (ck.name) {
			case RIFF_CASE ('l','t','x','t'):
				ltxt = calloc (1, sizeof (riff_ltxt_t));
				ltxt->ck = ck;
				read_ltxt (f, ck.len, &ltxt->ltxt);
				chunk = &ltxt->ck;
				break;
			case RIFF_CASE ('l','a','b','l'):
			case RIFF_CASE ('n','o','t','e'):
				label = malloc (sizeof (riff_label_t));
				label->ck = ck;
				if (!Rread (f, &label->ofs, 4)) {
					label->ofs = 0;
				}
				label->label = read_string (f, ck.len - 4);
				chunk = &label->ck;
				break;
			default:
				data = malloc (sizeof (riff_data_t));
				data->ck = ck;
				data->data = read_data (f, ck.len);
				chunk = &data->ck;
				break;
		}
		len -= ck.len + (ck.len & 1);
		dstring_append (list_buf, (char *)&chunk, sizeof (chunk));
		//FIXME list = (riff_list_t *) list_buf->str;
		chunk = 0;
	}
	dstring_append (list_buf, (char *)&chunk, sizeof (chunk));
	//FIXME list = (riff_list_t *) list_buf->str;
}

static riff_list_t *
read_list (riff_d_chunk_t *ck, QFile *f, int len)
{
	riff_d_chunk_t  *chunk = 0;
	dstring_t       *list_buf;
	riff_list_t     *list;

	list_buf = dstring_new ();
	list_buf->size = sizeof (riff_list_t);
	dstring_adjust (list_buf);
	list = (riff_list_t *)list_buf->str;
	list->ck = *ck;

	if (!Rread (f, list->name, sizeof (list->name))) {
		dstring_delete (list_buf);
		return 0;
	}
	len -= sizeof (list->name);
	while (len > 0) {
		RIFF_SWITCH (list->name) {
			case RIFF_CASE ('I','N','F','O'):
				{
					riff_data_t     *data = malloc (sizeof (riff_data_t));
					if (!Rread (f, &data->ck, sizeof (data->ck))) {
						len = 0;
						free (data);
						break;
					}
					chunk = &data->ck;
					data->ck.len = LittleLong (data->ck.len);
					//printf ("%.4s %d\n", data->ck.name, data->ck.len);
					len -= sizeof (data->ck);
					RIFF_SWITCH (data->ck.name) {
						case RIFF_CASE ('I','C','R','D'):
						case RIFF_CASE ('I','S','F','T'):
							data->data = read_string (f, data->ck.len);
							break;
						default:
							data->data = read_data (f, data->ck.len);
							break;
					}
					len -= data->ck.len + (data->ck.len & 1);
				}
				break;
			case RIFF_CASE ('a','d','t','l'):
				read_adtl (list_buf, f, len);
				len = 0;
				break;
			default:
				{
					riff_data_t     *data = malloc (sizeof (riff_data_t));
					if (!Rread (f, &data->ck, sizeof (data->ck))) {
						free (data);
						len = 0;
					} else {
						data->ck.len = LittleLong (data->ck.len);
						data->data = read_data (f, data->ck.len);
						len -= data->ck.len + sizeof (data->ck);
						chunk = &data->ck;
					}
				}
				break;
		}
		if (chunk) {
			dstring_append (list_buf, (char *)&chunk, sizeof (chunk));
			list = (riff_list_t *) list_buf->str;
		}
		chunk = 0;
	}
	dstring_append (list_buf, (char *)&chunk, sizeof (chunk));
	list = (riff_list_t *) list_buf->str;
	free (list_buf);
	return list;
}

static riff_d_cue_t *
read_cue (QFile *f, int len)
{
	riff_d_cue_t *cue = malloc (len);
	uint32_t    i;

	if (!Rread (f, cue, len)) {
		free (cue);
		cue = 0;
	} else {
		cue->count = LittleLong (cue->count);
		for (i = 0; i < cue->count; i++) {
			cue->cue_points[i].name = LittleLong (cue->cue_points[i].name);
			cue->cue_points[i].position = LittleLong (cue->cue_points[i].position);
			cue->cue_points[i].chunk_start = LittleLong (cue->cue_points[i].chunk_start);
			cue->cue_points[i].block_start = LittleLong (cue->cue_points[i].block_start);
			cue->cue_points[i].sample_offset = LittleLong (cue->cue_points[i].sample_offset);
		}
	}

	return cue;
}

VISIBLE riff_t *
riff_read (QFile *f)
{
	dstring_t  *riff_buf;
	riff_list_t     *riff;
	riff_d_chunk_t  *chunk = 0;
	riff_d_chunk_t   ck;
	int         file_len, len;

	riff_buf = dstring_new ();
	riff_buf->size = sizeof (riff_list_t);
	dstring_adjust (riff_buf);
	riff = (riff_list_t *)riff_buf->str;

	file_len = Qfilesize (f);

	if (!Rread (f, &riff->ck, sizeof (riff->ck))) {
		dstring_delete (riff_buf);
		return 0;
	}
	if (!Rread (f, riff->name, sizeof (riff->name))) {
		dstring_delete (riff_buf);
		return 0;
	}
	if (strncmp (riff->ck.name, "RIFF", 4) || strncmp (riff->name, "WAVE", 4)) {
		dstring_delete (riff_buf);
		return 0;
	}
	//FIXME the pos test should be in quakeio
	while (Qtell (f) < file_len && Rread (f, &ck, sizeof (ck))) {
		ck.len = LittleLong (ck.len);
		//printf ("%.4s %d\n", ck.name, ck.len);
		if (ck.len < 0x80000000)
			len = ck.len;
		else {
			//puts ("bling");
			ck.len = len = file_len - Qtell (f);
		}
		RIFF_SWITCH (ck.name) {
			case RIFF_CASE ('c','u','e',' '):
				{
					riff_cue_t      *cue = malloc (sizeof (riff_cue_t));
					cue->ck = ck;
					cue->cue = read_cue (f, len);
					chunk = &cue->ck;
				}
				break;
			case RIFF_CASE ('L','I','S','T'):
				{
					riff_list_t     *list;
					list = read_list (&ck, f, len);
					chunk = &list->ck;
				}
				break;
			case RIFF_CASE ('f','m','t',' '):
				{
					riff_format_t *fmt;
					fmt = malloc (sizeof (riff_format_t) + ck.len);
					fmt->ck = ck;
					if (!Rread (f, fmt->fdata, ck.len)) {
						free (fmt);
					} else {
						riff_d_format_t *fd = (riff_d_format_t *) fmt->fdata;

						chunk = &fmt->ck;
						fd->format_tag = LittleShort (fd->format_tag);
						fd->channels = LittleShort (fd->channels);
						fd->samples_per_sec = LittleLong (fd->samples_per_sec);
						fd->bytes_per_sec = LittleLong (fd->bytes_per_sec);
						fd->align = LittleShort (fd->align);
						if (fd->format_tag == 1)
							fd->bits_per_sample =
								LittleShort (fd->bits_per_sample);
					}
				}
				break;
			case RIFF_CASE ('d','a','t','a'):
				{
					riff_data_t     *data = malloc (sizeof (riff_data_t));
					int         c;

					data->ck = ck;
					data->data = malloc (sizeof (int));
					*((int *)data->data) = Qtell (f);
					Qseek (f, ck.len, SEEK_CUR);
					if ((c = Qgetc (f)) && c != -1)
						Qungetc (f, c);
					chunk = &data->ck;
				}
				break;
			case RIFF_CASE ('w','a','v','l'):
				// FIXME: Convert wavl to data ?
			case RIFF_CASE ('s','l','n','t'):
				// FIXME: Add silence to data
			case RIFF_CASE ('l','i','s','t'):
			case RIFF_CASE ('l','a','b','l'):
			case RIFF_CASE ('n','o','t','e'):
			case RIFF_CASE ('l','t','x','t'):
			case RIFF_CASE ('p','l','s','t'):
			case RIFF_CASE ('i','n','s','t'):
			case RIFF_CASE ('f','a','c','t'):
			case RIFF_CASE ('s','m','p','l'):
				{	// Unused chunk, still present in a lot of wav files.
					int c;

					Qseek(f, ck.len, SEEK_CUR);
					if ((c = Qgetc (f)) && c != -1)
						Qungetc (f, c);
					continue; // Skip those blocks.
				}
				break;
			default:
				// unknown chunk. bail (could be corrupted file)
				chunk = 0;
				goto bail;
				break;
		}
		dstring_append (riff_buf, (char *)&chunk, sizeof (chunk));
		chunk = 0;
	}
bail:
	dstring_append (riff_buf, (char *)&chunk, sizeof (chunk));
	riff = (riff_list_t *) riff_buf->str;
	free (riff_buf);
	return riff;
}

/*****************************************************************************
*****************************************************************************/

static void
free_adtl (riff_d_chunk_t *adtl)
{
	riff_ltxt_t     *ltxt;
	riff_label_t    *label;
	riff_data_t     *data;

	//printf ("    %.4s\n", adtl->name);
	RIFF_SWITCH (adtl->name) {
		case RIFF_CASE ('l','t','x','t'):
			ltxt = (riff_ltxt_t *) adtl;
			/*printf ("      %d %d %4s %d %d %d %d\n",
					ltxt->ltxt.name,
					ltxt->ltxt.len,
					ltxt->ltxt.purpose,
					ltxt->ltxt.country,
					ltxt->ltxt.language,
					ltxt->ltxt.dialect,
					ltxt->ltxt.codepage);*/
			free (ltxt);
			break;
		case RIFF_CASE ('l','a','b','l'):
		case RIFF_CASE ('n','o','t','e'):
			label = (riff_label_t *) adtl;
			//printf ("      %-8d %s\n", label->ofs, label->label);
			if (label->label)
				free (label->label);
			free (label);
			break;
		default:
			data = (riff_data_t *) adtl;
			if (data->data)
				free (data->data);
			free (data);
			break;
	}
}

static void
free_list (riff_list_t *list)
{
	riff_d_chunk_t **ck;
	riff_data_t     *data;

	//printf ("  %.4s\n", list->name);
	for (ck = list->chunks; *ck; ck++) {
		RIFF_SWITCH (list->name) {
			case RIFF_CASE ('I','N','F','O'):
				data = (riff_data_t *) *ck;
				//printf ("    %.4s\n", data->ck.name);
				RIFF_SWITCH (data->ck.name) {
					case RIFF_CASE ('I','C','R','D'):
					case RIFF_CASE ('I','S','F','T'):
						//printf ("      %s\n", data->data);
					default:
						if (data->data)
							free (data->data);
						free (data);
						break;
				}
				break;
			case RIFF_CASE ('a','d','t','l'):
				free_adtl (*ck);
				break;
			default:
				data = (riff_data_t *) *ck;
				if (data->data)
					free (data->data);
				free (data);
				break;
		}
	}
	free (list);
}

VISIBLE void
riff_free (riff_t *riff)
{
	riff_d_chunk_t **ck;
	riff_cue_t      *cue;
	riff_list_t     *list;
	riff_data_t     *data;

	for (ck = riff->chunks; *ck; ck++) {
		//printf ("%.4s\n", (*ck)->name);
		RIFF_SWITCH ((*ck)->name) {
			case RIFF_CASE ('c','u','e',' '):
				cue = (riff_cue_t *) *ck;
				if (cue->cue) {
					/*int i;
					for (i = 0; i < cue->cue->count; i++) {
						printf ("  %08x %d %.4s %d %d %d\n",
								cue->cue->cue_points[i].name,
								cue->cue->cue_points[i].position,
								cue->cue->cue_points[i].chunk,
								cue->cue->cue_points[i].chunk_start,
								cue->cue->cue_points[i].block_start,
								cue->cue->cue_points[i].sample_offset);
					}*/
					free (cue->cue);
				}
				free (cue);
				break;
			case RIFF_CASE ('L','I','S','T'):
				list = (riff_list_t *) *ck;
				free_list (list);
				break;
			case RIFF_CASE ('f','m','t',' '):
				free (*ck);
				break;
			default:
				data = (riff_data_t *) *ck;
				if (data->data)
					free (data->data);
				free (data);
				break;
		}
	}
	free (riff);
}
