#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#include <stdlib.h>

#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfbsp/include/brush.h"
#include "tools/qfbsp/include/bsp5.h"
#include "tools/qfbsp/include/draw.h"
#include "tools/qfbsp/include/options.h"

typedef struct lumpinfo_s {
	const char *name;
	int         data;
	int         size;
	const char *(*extra) (const void *data);
} lumpinfo_t;

static const char *
num_textures (const void *data)
{
	__auto_type d = (const dmiptexlump_t *)data;
	return va (0, " %7d", LittleLong (d->nummiptex));
}

#define O(f) field_offset (bsp_t, f)
static lumpinfo_t lump_info[] = {
	{ "entities",     O(entdata),       O(entdatasize) },
	{ "planes",       O(planes),        O(numplanes) },
	{ "textures",     O(texdata),       O(texdatasize), num_textures },
	{ "vertices",     O(vertexes),      O(numvertexes) },
	{ "visibility",   O(visdata),       O(visdatasize) },
	{ "nodes",        O(nodes),         O(numnodes) },
	{ "texinfo",      O(texinfo),       O(numtexinfo) },
	{ "faces",        O(faces),         O(numfaces) },
	{ "lighting",     O(lightdata),     O(lightdatasize) },
	{ "clipnodes",    O(clipnodes),     O(numclipnodes) },
	{ "leafs",        O(leafs),         O(numleafs) },
	{ "marksurfaces", O(marksurfaces),  O(nummarksurfaces) },
	{ "edges",        O(edges),         O(numedges) },
	{ "surfedges",    O(surfedges),     O(numsurfedges) },
	{ "models",       O(models),        O(nummodels) },
};

void
bspinfo ()
{
	printf ("version: %x\n", bsp->header->version);
	for (int i = 0; i < HEADER_LUMPS; i++) {
		lump_t     *lump = &bsp->header->lumps[i];
		lumpinfo_t *info = &lump_info[i];
		const void *data = *(void **)((byte *) bsp + info->data);
		size_t     *size = (size_t *)((byte *) bsp + info->size);
		const char *extra = "";

		if (info->extra) {
			extra = info->extra (data);
		}

		printf ("    %-12s: %7d %7d %7zd%s\n", info->name,
				lump->fileofs, lump->filelen, *size, extra);

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
