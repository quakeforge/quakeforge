#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#include <stdlib.h>

#include "QF/sys.h"

#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/draw.h"
#include "tools/qfbsp/include/options.h"

typedef struct lumpinfo_s {
	const char *name;
	int         size;
} lumpinfo_t;

#define O(f) field_offset (bsp_t, f)
static lumpinfo_t lump_info[] = {
	{ "entities",     O(entdatasize) },
	{ "planes",       O(numplanes) },
	{ "textures",     O(texdatasize) },
	{ "vertices",     O(numvertexes) },
	{ "visibility",   O(visdatasize) },
	{ "nodes",        O(numnodes) },
	{ "texinfo",      O(numtexinfo) },
	{ "faces",        O(numfaces) },
	{ "lighting",     O(lightdatasize) },
	{ "clipnodes",    O(numclipnodes) },
	{ "leafs",        O(numleafs) },
	{ "marksurfaces", O(nummarksurfaces) },
	{ "edges",        O(numedges) },
	{ "surfedges",    O(numsurfedges) },
	{ "models",       O(nummodels) },
};

void
bspinfo ()
{
	printf ("version: %x\n", bsp->header->version);
	for (int i = 0; i < HEADER_LUMPS; i++) {
		lump_t     *lump = &bsp->header->lumps[i];
		lumpinfo_t *info = &lump_info[i];
		size_t     *size = (size_t *)((byte *) bsp + info->size);

		printf ("    %-12s: %7d %7d %7zd\n", info->name,
				lump->fileofs, lump->filelen, *size);

	}
	for (unsigned i = 0; i < bsp->nummodels; i++) {
		dmodel_t   *model = &bsp->models[i];
		printf ("model: *%d\n", i);
		printf ("    mins    : [%g, %g, %g]\n", VectorExpand (model->mins));
		printf ("    maxs    : [%g, %g, %g]\n", VectorExpand (model->maxs));
		printf ("    origin  : [%g, %g, %g]\n", VectorExpand (model->origin));
		printf ("    headnode:");
		for (int j = 0; j < MAX_MAP_HULLS; j++) {
			printf (" %d", model->headnode[j]);
		}
		printf ("\n");
		printf ("    visleafs: %d\n", model->visleafs);
		printf ("    faces   : %7d %7d\n", model->firstface, model->numfaces);
	}
}
