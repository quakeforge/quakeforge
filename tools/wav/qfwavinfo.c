#include <stdio.h>
#include <stdlib.h>

#include "QF/dstring.h"
#include "QF/riff.h"

int
main (int argc, char **argv)
{

	while (*++argv) {
		QFile      *f;
		riff_t     *riff = 0;
		riff_d_chunk_t **ck;
		int         sample_start, sample_count;

		//puts (*argv);
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
			RIFF_SWITCH ((*ck)->name) {
				case RIFF_CASE ('f', 'm', 't', ' '):
					/*{
						riff_format_t *_fmt = (riff_format_t *) *ck;
						riff_d_format_t *fmt = (riff_d_format_t *) _fmt->fdata;

						printf ("fmt.format_tag      = %d\n", fmt->format_tag);
						printf ("fmt.channels        = %d\n", fmt->channels);
						printf ("fmt.samples_per_sec = %d\n", fmt->samples_per_sec);
						printf ("fmt.bytes_per_sec   = %d\n", fmt->bytes_per_sec);
						printf ("fmt.align           = %d\n", fmt->align);
						if (fmt->format_tag == 1)
							printf ("fmt.bits_per_sample = %d\n", fmt->bits_per_sample);
					}*/
					break;
				case RIFF_CASE ('c', 'u', 'e', ' '):
					{
						riff_cue_t  *cue = (riff_cue_t *)*ck;
						riff_d_cue_t *dcue = cue->cue;
						riff_d_cue_point_t *cp = dcue->cue_points;
						unsigned int i;

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
				case RIFF_CASE ('L','I','S','T'):
					{
						riff_list_t     *list = (riff_list_t *)*ck;
						RIFF_SWITCH (list->name) {
							case RIFF_CASE ('a','d','t','l'):
								{
									riff_d_chunk_t **ck;
									for (ck = list->chunks; *ck; ck++) {
										RIFF_SWITCH ((*ck)->name) {
											case RIFF_CASE ('l', 't', 'x', 't'):
												{
													riff_ltxt_t     *ltxt = (riff_ltxt_t *)*ck;
													riff_d_ltxt_t   *dltxt = &ltxt->ltxt;
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
				case RIFF_CASE ('d','a','t','a'):
#if 0
					printf ("data: %d %d\n", *(int*)((data_t*)(*ck))->data, (*ck)->len);
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
