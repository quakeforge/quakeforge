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

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
void *alloca(size_t size);
#endif

#include "QF/dstring.h"
#include "QF/riff.h"

static char *
read_string (QFile *f, int len)
{
	char        c[2] = {0, 0};
	char       *s;
	int         l = len;
	dstring_t  *str = dstring_newstr ();

	while (l--) {
		Qread (f, c, 1);
		if (!c[0])
			break;
		dstring_appendstr (str, c);
	}
	s = str->str;
	free (str);
	if (len &1) puts ("bling");
	Qseek (f, l + (len & 1), SEEK_CUR);
	return s;
}

static void *
read_data (QFile *f, int len)
{
	void       *data = malloc (len);

	Qread (f, data, len);
	if (len &1) puts ("bling");
	Qseek (f, len & 1, SEEK_CUR);
	return data;
}

static void
read_ltxt (QFile *f, int len, d_ltxt_t *ltxt)
{
	Qread (f, ltxt, len);
}

static void
read_adtl (dstring_t *list_buf, QFile *f, int len)
{
	d_chunk_t   ck, *chunk = 0;
	ltxt_t     *ltxt;
	label_t    *label;
	data_t     *data;
	list_t     *list;
	int         r;

	list = (list_t *) list_buf->str;
	while (len) {
		r = Qread (f, &ck, sizeof (ck));
		if (!r) {
			len = 0;
			break;
		}
		len -= r;
		SWITCH (ck.name) {
			case CASE ('l','t','x','t'):
				ltxt = malloc (sizeof (ltxt_t));
				ltxt->ck = ck;
				read_ltxt (f, ck.len, &ltxt->ltxt);
				chunk = &ltxt->ck;
				break;
			case CASE ('l','a','b','l'):
			case CASE ('n','o','t','e'):
				label = malloc (sizeof (label_t));
				label->ck = ck;
				Qread (f, &label->ofs, 4);
				label->label = read_string (f, ck.len - 4);
				chunk = &label->ck;
				break;
			default:
				data = malloc (sizeof (data));
				data->ck = ck;
				data->data = read_data (f, ck.len);
				chunk = &data->ck;
				break;
		}
		len -= ck.len + (ck.len & 1);
		dstring_append (list_buf, (char *)&chunk, sizeof (chunk));
		list = (list_t *) list_buf->str;
		chunk = 0;
	}
	dstring_append (list_buf, (char *)&chunk, sizeof (chunk));
	list = (list_t *) list_buf->str;
}

static list_t *
read_list (d_chunk_t *ck, QFile *f, int len)
{
	d_chunk_t  *chunk = 0;
	dstring_t  *list_buf;
	list_t     *list;
	int         r;

	list_buf = dstring_new ();
	list_buf->size = sizeof (list_t);
	dstring_adjust (list_buf);
	list = (list_t *)list_buf->str;
	list->ck = *ck;

	len -= Qread (f, list->name, sizeof (list->name));
	while (len) {
		SWITCH (list->name) {
			case CASE ('I','N','F','O'):
				{
					data_t     *data = malloc (sizeof (data_t));
					chunk = &data->ck;
					r = Qread (f, &data->ck, sizeof (data->ck));
					if (!r) {
						len = 0;
						break;
					}
					len -= r;
					SWITCH (data->ck.name) {
						case CASE ('I','C','R','D'):
						case CASE ('I','S','F','T'):
							data->data = read_string (f, data->ck.len);
							break;
						default:
							data->data = read_data (f, data->ck.len);
							break;
					}
					len -= data->ck.len + (data->ck.len & 1);
				}
				break;
			case CASE ('a','d','t','l'):
				read_adtl (list_buf, f, len);
				len = 0;
				break;
			default:
				{
					data_t     *data = malloc (sizeof (data_t));
					Qread (f, &data->ck, sizeof (data->ck));
					data->data = read_data (f, data->ck.len);
					len -= data->ck.len + sizeof (data->ck);
					chunk = &data->ck;
				}
				len = 0;
				break;
		}
		if (chunk) {
			dstring_append (list_buf, (char *)&chunk, sizeof (chunk));
			list = (list_t *) list_buf->str;
		}
		chunk = 0;
	}
	dstring_append (list_buf, (char *)&chunk, sizeof (chunk));
	list = (list_t *) list_buf->str;
	free (list_buf);
	return list;
}

static d_cue_t *
read_cue (QFile *f, int len)
{
	d_cue_t    *cue = malloc (len);

	Qread (f, cue, len);

	return cue;
}

list_t *
riff_read (QFile *f)
{
	dstring_t  *riff_buf;
	list_t     *riff;
	d_chunk_t  *chunk = 0;
	d_chunk_t   ck;
	int         file_len, len;

	riff_buf = dstring_new ();
	riff_buf->size = sizeof (list_t);
	dstring_adjust (riff_buf);
	riff = (list_t *)riff_buf->str;

	Qseek (f, 0, SEEK_END);
	file_len = Qtell (f);
	Qseek (f, 0, SEEK_SET);

	Qread (f, &riff->ck, sizeof (riff->ck));
	Qread (f, riff->name, sizeof (riff->name));
	while (Qread (f, &ck, sizeof (ck))) {
		if (ck.len < 0x80000000)
			len = ck.len;
		else
			len = file_len - Qtell (f);
		SWITCH (ck.name) {
			case CASE ('c','u','e',' '):
				{
					cue_t      *cue = malloc (sizeof (cue_t));
					cue->ck = ck;
					cue->cue = read_cue (f, len);
					chunk = &cue->ck;
				}
				break;
			case CASE ('L','I','S','T'):
				{
					list_t     *list;
					list = read_list (&ck, f, len);
					chunk = &list->ck;
				}
				break;
			default:
				{
					data_t     *data = malloc (sizeof (data_t));
					data->ck = ck;
					data->data = read_data (f, len);
					chunk = &data->ck;
				}
				break;
		}
		dstring_append (riff_buf, (char *)&chunk, sizeof (chunk));
		riff = (list_t *) riff_buf->str;
		chunk = 0;
	}
	dstring_append (riff_buf, (char *)&chunk, sizeof (chunk));
	riff = (list_t *) riff_buf->str;
	free (riff_buf);
	return riff;
}

static void
free_adtl (d_chunk_t *adtl)
{
	ltxt_t     *ltxt;
	label_t    *label;
	data_t     *data;

	//printf ("    %.4s\n", adtl->name);
	SWITCH (adtl->name) {
		case CASE ('l','t','x','t'):
			ltxt = (ltxt_t *) adtl;
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
		case CASE ('l','a','b','l'):
		case CASE ('n','o','t','e'):
			label = (label_t *) adtl;
			//printf ("      %-8d %s\n", label->ofs, label->label);
			free (label->label);
			free (label);
			break;
		default:
			data = (data_t *) adtl;
			free (data->data);
			free (data);
			break;
	}
}

static void
free_list (list_t *list)
{
	d_chunk_t **ck;
	data_t     *data;

	//printf ("  %.4s\n", list->name);
	for (ck = list->chunks; *ck; ck++) {
		SWITCH (list->name) {
			case CASE ('I','N','F','O'):
				data = (data_t *) *ck;
				//printf ("    %.4s\n", data->ck.name);
				SWITCH (data->ck.name) {
					case CASE ('I','C','R','D'):
					case CASE ('I','S','F','T'):
						//printf ("      %s\n", data->data);
					default:
						free (data->data);
						free (data);
						break;
				}
				break;
			case CASE ('a','d','t','l'):
				free_adtl (*ck);
				break;
			default:
				data = (data_t *) *ck;
				free (data->data);
				free (data);
				break;
		}
	}
	free (list);
}

void
riff_free (list_t *riff)
{
	d_chunk_t **ck;
	cue_t      *cue;
	list_t     *list;
	data_t     *data;

	for (ck = riff->chunks; *ck; ck++) {
		//printf ("%.4s\n", (*ck)->name);
		SWITCH ((*ck)->name) {
			case CASE ('c','u','e',' '):
				cue = (cue_t *) *ck;
				/*{
					int i;
					for (i = 0; i < cue->cue->count; i++) {
						printf ("  %08x %d %.4s %d %d %d\n",
								cue->cue->cue_points[i].name,
								cue->cue->cue_points[i].position,
								cue->cue->cue_points[i].chunk,
								cue->cue->cue_points[i].chunk_start,
								cue->cue->cue_points[i].block_start,
								cue->cue->cue_points[i].sample_offset);
					}
				}*/
				free (cue->cue);
				free (cue);
				break;
			case CASE ('L','I','S','T'):
				list = (list_t *) *ck;
				free_list (list);
				break;
			default:
				data = (data_t *) *ck;
				free (data->data);
				free (data);
				break;
		}
	}
	free (riff);
}
