#include <stdio.h>
#include <stdlib.h>

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
	d_cue_t     cue;
} cue_t;

typedef struct format_s {
	d_chunk_t   ck;
	d_format_t  format;
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

#define SWITCH(name) switch (((name)[0] << 24) | ((name)[1] << 16) \
							 | ((name)[2] << 8) | (name)[3])
#define CASE(a,b,c,d) (((unsigned char)(a) << 24) \
					   | ((unsigned char)(b) << 16) \
					   | ((unsigned char)(c) << 8) \
					   | (unsigned char)(d))

char *
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

void *
read_data (FILE *f, int len)
{
	void       *data = malloc (len);

	fread (data, 1, 1, f);
	fseek (f, len & 1, SEEK_CUR);
	return data;
}

void
read_ltxt (FILE *f, int len, d_ltxt_t *ltxt)
{
	fread (ltxt, 1, len, f);
}

void
read_adtl (FILE *f, int len)
{
	d_chunk_t   ck;
	ltxt_t     *ltxt;
	label_t    *label;
	data_t     *data;
	int         r;

	while (len) {
		r = fread (&ck, 1, sizeof (ck), f);
		if (!r) {
			len = 0;
			break;
		}
		len -= r;
		printf ("                '%4.4s' %6d\n", ck.name, ck.len);
		SWITCH (ck.name) {
			case CASE ('l','t','x','t'):
				ltxt = malloc (sizeof (ltxt_t));
				ltxt->ck = ck;
				read_ltxt (f, ck.len, &ltxt->ltxt);
				break;
			case CASE ('l','a','b','l'):
			case CASE ('n','o','t','e'):
				label = malloc (sizeof (label_t));
				label->ck = ck;
				fread (&label->ofs, 1, 4, f);
				label->label = read_string (f, ck.len - 4);
				break;
			default:
				data = malloc (sizeof (data));
				data->ck = ck;
				data->data = read_data (f, ck.len);
				break;
		}
		len -= ck.len + (ck.len & 1);
	}
}

void
read_list (FILE *f, int len)
{
	d_chunk_t   ck;
	unsigned char name[4];
	int         r;

	len -= fread (name, 1, sizeof (name), f);
	printf ("            '%4.4s' %6d\n", name, len);
	while (len) {
		SWITCH (name) {
			case CASE ('I','N','F','O'):
				r = fread (&ck, 1, sizeof (ck), f);
				if (!r) {
					len = 0;
					break;
				}
				len -= r;
				printf ("                '%4.4s' %6d\n", ck.name, ck.len);
				SWITCH (ck.name) {
					case CASE ('I','C','R','D'):
					case CASE ('I','S','F','T'):
						read_string (f, ck.len);
						break;
					default:
						read_data (f, ck.len);
						//fseek (f, ck.len + (ck.len & 1), SEEK_CUR);
						break;
				}
				len -= ck.len + (ck.len & 1);
				break;
			case CASE ('a','d','t','l'):
				read_adtl (f, len);
				len = 0;
			default:
				read_data (f, len);
				len = 0;
				break;
		}
	}
}

void
read_cue (FILE *f, int len)
{
	d_cue_t    *cue = alloca (len);
	d_cue_point_t *cp;
	int         i;

	fread (cue, 1, len, f);
	printf ("            %d\n", cue->count);
	for (i = 0, cp = cue->cue_points; i < cue->count; i++) {
		printf ("            %8x %6d %4.4s %d %d %6d\n", cp->name, cp->position,
				cp->chunk, cp->chunk_start, cp->block_start,
				cp->sample_offset);
	}
}

int
main (int argc, char **argv)
{
	while (*++argv) {
		FILE       *f = fopen (*argv, "rb");
		d_chunk_t   ck;
		char        name[4];
		int         file_len, len;

		if (f) {
			fseek (f, 0, SEEK_END);
			file_len = ftell (f);
			fseek (f, 0, SEEK_SET);
			printf ("%s\n", *argv);
			fread (&ck, 1, sizeof (ck), f);
			printf ("'%4.4s' %d\n", ck.name, ck.len);
			fread (name, 1, sizeof (name), f);
			printf ("    '%4.4s'\n", name);
			while (fread (&ck, 1, sizeof (ck), f)) {
				if (ck.len < 0x80000000)
					len = ck.len;
				else
					len = file_len - ftell (f);
				printf ("        '%4.4s' %6d\n", ck.name, len);
				SWITCH (ck.name) {
					case CASE ('c','u','e',' '):
						read_cue (f, len);
						break;
					case CASE ('L','I','S','T'):
						read_list (f, len);
						break;
					default:
						fseek (f, len + (len & 1), SEEK_CUR);
						break;
				}
			}
			fclose (f);
		}
	}
	return 0;
}
