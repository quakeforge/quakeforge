#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
void *alloca(size_t size);
#endif

#include "QF/dstring.h"

typedef struct d_chunk_s {
	unsigned char name[4];
	unsigned    len;
} d_chunk_t;

typedef struct d_cue_point_s {
	unsigned    name;
	unsigned    position;
	char        chunk[4];
	unsigned    chunk_start;
	unsigned    block_start;
	unsigned    sample_offset;
} d_cue_point_t;

typedef struct d_cue_s {
	unsigned    count;
	d_cue_point_t cue_points[1];
} d_cue_t;

typedef struct d_format_s {
	unsigned short format_tag;
	unsigned short channels;
	unsigned    samples_pre_sec;
	unsigned    byte_per_sec;
	unsigned short align;
} d_format_t;

typedef struct d_ltxt_s {
	unsigned    name;
	unsigned    len;
	char        purpose[4];
	unsigned    country;
	unsigned    language;
	unsigned    dialect;
	unsigned    codepage;
	unsigned char data[0];
} d_ltxt_t;

typedef struct cue_s {
	d_chunk_t   ck;
	d_cue_t    *cue;
} cue_t;

typedef struct format_s {
	d_chunk_t   ck;
	d_format_t  format;
	char        fdata[0];
} format_t;

typedef struct ltxt_s {
	d_chunk_t   ck;
	d_ltxt_t    ltxt;
} ltxt_t;

typedef struct label_s {
	d_chunk_t   ck;
	unsigned    ofs;
	char       *label;
} label_t;

typedef struct data_s {
	d_chunk_t   ck;
	char       *data;
} data_t;

typedef struct list_s {
	d_chunk_t   ck;
	char        name[4];
	d_chunk_t  *chunks[0];
} list_t;

#define SWITCH(name) switch (((name)[0] << 24) | ((name)[1] << 16) \
							 | ((name)[2] << 8) | (name)[3])
#define CASE(a,b,c,d) (((unsigned char)(a) << 24) \
					   | ((unsigned char)(b) << 16) \
					   | ((unsigned char)(c) << 8) \
					   | (unsigned char)(d))

static char *
read_string (FILE *f, int len)
{
	char        c[2] = {0, 0};
	char       *s;
	int         l = len;
	dstring_t  *str = dstring_newstr ();

	while (l--) {
		fread (c, 1, 1, f);
		if (!c[0])
			break;
		dstring_appendstr (str, c);
	}
	s = str->str;
	free (str);
	fseek (f, l + (len & 1), SEEK_CUR);
	return s;
}

static void *
read_data (FILE *f, int len)
{
	void       *data = malloc (len);

	fread (data, 1, len, f);
	fseek (f, len & 1, SEEK_CUR);
	return data;
}

static void
read_ltxt (FILE *f, int len, d_ltxt_t *ltxt)
{
	fread (ltxt, 1, len, f);
}

static void
read_adtl (dstring_t *list_buf, FILE *f, int len)
{
	d_chunk_t   ck, *chunk = 0;
	ltxt_t     *ltxt;
	label_t    *label;
	data_t     *data;
	list_t     *list;
	int         r;

	list = (list_t *) list_buf->str;
	while (len) {
		r = fread (&ck, 1, sizeof (ck), f);
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
				fread (&label->ofs, 1, 4, f);
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
read_list (d_chunk_t *ck, FILE *f, int len)
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

	len -= fread (list->name, 1, sizeof (list->name), f);
	while (len) {
		SWITCH (list->name) {
			case CASE ('I','N','F','O'):
				{
					data_t     *data = malloc (sizeof (data_t));
					chunk = &data->ck;
					r = fread (&data->ck, 1, sizeof (data->ck), f);
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
					fread (&data->ck, 1, sizeof (data->ck), f);
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
	return list;
}

static d_cue_t *
read_cue (FILE *f, int len)
{
	d_cue_t    *cue = malloc (len);

	fread (cue, 1, len, f);

	return cue;
}

static list_t *
read_riff (const char *filename)
{
	dstring_t  *riff_buf;
	list_t     *riff = 0;
	d_chunk_t  *chunk = 0;
	FILE       *f = fopen (filename, "rb");
	d_chunk_t   ck;
	int         file_len, len;

	if (f) {
		riff_buf = dstring_new ();
		riff_buf->size = sizeof (list_t);
		dstring_adjust (riff_buf);
		riff = (list_t *)riff_buf->str;

		fseek (f, 0, SEEK_END);
		file_len = ftell (f);
		fseek (f, 0, SEEK_SET);

		fread (&riff->ck, 1, sizeof (riff->ck), f);
		fread (riff->name, 1, sizeof (riff->name), f);
		while (fread (&ck, 1, sizeof (ck), f)) {
			if (ck.len < 0x80000000)
				len = ck.len;
			else
				len = file_len - ftell (f);
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
		fclose (f);
	}
	return riff;
}

int
main (int argc, char **argv)
{

	while (*++argv) {
		list_t     *riff = read_riff (*argv);
		d_chunk_t **ck;
		int         sample_start, sample_count;

		if (!riff) {
			fprintf (stderr, "couldn't read %s\n", *argv);
			continue;
		}
		sample_start = -1;
		sample_count = -1;
		for (ck = riff->chunks; *ck; ck++) {
			SWITCH ((*ck)->name) {
				case CASE ('c', 'u', 'e', ' '):
					{
						cue_t      *cue = (cue_t *)*ck;
						d_cue_t    *dcue = cue->cue;
						d_cue_point_t *cp = dcue->cue_points;
						int         i;

						for (i = 0; i < dcue->count; i++)
							sample_start = cp[i].sample_offset;
#if 0
							printf ("cuepoint: %d %d %.4s %d %d %d\n",
									cp[i].name,
									cp[i].position,
									cp[i].chunk,
									cp[i].chunk_start,
									cp[i].block_start,
									cp[i].sample_offset);
#endif
					}
					break;
				case CASE ('L','I','S','T'):
					{
						list_t     *list = (list_t *)*ck;
						SWITCH (list->name) {
							case CASE ('a','d','t','l'):
								{
									d_chunk_t **ck;
									for (ck = list->chunks; *ck; ck++) {
										SWITCH ((*ck)->name) {
											case CASE ('l', 't', 'x', 't'):
												{
													ltxt_t     *ltxt = (ltxt_t *)*ck;
													d_ltxt_t   *dltxt = &ltxt->ltxt;
													sample_count = dltxt->len;
#if 0
													printf ("ltxt: %d %d %4s %d %d %d %d\n",
															dltxt->name,
															dltxt->len,
															dltxt->purpose,
															dltxt->country,
															dltxt->language,
															dltxt->dialect,
															dltxt->codepage);
#endif
												}
												break;
										}
									}
								}
								break;
						}
					}
					break;
				case CASE ('d','a','t','a'):
#if 0
					printf ("data: %d\n", (*ck)->len);
#endif
					sample_count = (*ck)->len;
					break;
			}
		}
		if (sample_start!= -1)
			printf ("CUEPOINT=%d %d\n", sample_start, sample_count);
	}
	return 0;
}
