#include <stdio.h>
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/riff.h"

int
main (int argc, char **argv)
{

	while (*++argv) {
		QFile      *f;
		list_t     *riff = 0;
		d_chunk_t **ck;
		int         sample_start, sample_count;
puts (*argv);
		f = Qopen (*argv, "rbz");
		if (f) {
			riff = riff_read (f);
			Qclose (f);
		}
		if (!f || !riff) {
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
		riff_free (riff);
	}
	return 0;
}
