#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>

#include "QF/ui/view.h"
#include "QF/ui/passage.h"
#include "QF/ecs/component.h"

static const char test_text[] = {
	"Guarding the entrance to the Grendal "
	"Gorge is the Shadow Gate, a small keep "
	"and monastary which was once the home "
	"of the Shadow cult.\n"
	"  For years the Shadow Gate existed in "
	"obscurity but after the cult discovered "
	"the \u00c2\u00ec\u00e1\u00e3\u00eb\u2002\u00c7\u00e1\u00f4\u00e5 "
	"in the caves below the empire took notice. "
	"A batallion of Imperial Knights were "
	"sent to the gate to destroy the cult "
	"and claim the artifact for the King.\nasdf",
};

static int __attribute__((pure))
check_non_space (const char *text, psg_text_t *to)
{
	int         size;

	for (uint32_t offs = 0; offs < to->size; offs += size) {
		if (!(size = Passage_IsSpace (text + to->text + offs))) {
			return 1;
		}
	}
	return 0;
}

static int __attribute__((pure))
check_space_or_nl (const char *text, psg_text_t *to)
{
	for (uint32_t offs = 0; offs < to->size; offs++) {
		if (text[to->text + offs] == '\n'
			|| Passage_IsSpace (text + to->text + offs)) {
			return 1;
		}
	}
	return 0;
}

int
main (void)
{
	int         ret = 0;
	ecs_system_t psg_sys = {
		.reg = ECS_NewRegistry ("passage"),
		.base = ECS_RegisterComponents (psg_sys.reg, passage_components,
										passage_comp_count),
	};
	ECS_CreateComponentPools (psg_sys.reg);

	passage_t  *passage = Passage_New (psg_sys);
	Passage_ParseText (passage, test_text);
	hierarchy_t *h = Ent_GetComponent (passage->hierarchy, ecs_hierarchy,
									   passage->reg);
	if (h->childCount[0] != 3) {
		ret = 1;
		printf ("incorrect number of paragraphs: %d\n", h->childCount[0]);
	}
	if (h->num_objects != 144) {
		ret = 1;
		printf ("incorrect number of text objects: %d\n", h->num_objects);
	}
	if (h->childCount[1] != 49) {
		ret = 1;
		printf ("incorrect number of text objects in first paragraph: %d\n",
				h->childCount[1]);
	}
	if (h->childCount[2] != 90) {
		ret = 1;
		printf ("incorrect number of text objects in second paragraph: %d\n",
				h->childCount[2]);
	}
	if (h->childCount[3] != 1) {
		ret = 1;
		printf ("incorrect number of text objects in third paragraph: %d\n",
				h->childCount[3]);
	}
	uint32_t   *childIndex = h->childIndex;
	psg_text_t *text_objs = h->components[0];
	psg_text_t *to = &text_objs[childIndex[2] + 0];
	if (to->size != 2 && (passage->text[to->text] != ' '
						  && passage->text[to->text + 1] != ' ')) {
		ret = 1;
		printf ("second paragram does not begin with double space: %d '%.*s'\n",
				to->size, to->size, passage->text + to->text);
	}
	//if (text_view->bol_suppress) {
	//	ret = 1;
	//	printf ("second paragram indent suppressed\n");
	//}
	for (uint32_t i = 0; i < h->childCount[0]; i++) {
		for (uint32_t j = 0; j < h->childCount[1 + i]; j++) {
			psg_text_t *to = &text_objs[childIndex[1 + i] + j];
			unsigned    is_space = Passage_IsSpace (passage->text + to->text);
			if (i == 1 && j == 0) {
				// second paragraph indent, tested above
				continue;
			}
			//if ((!!is_space) != text_view->bol_suppress) {
			//	ret = 1;
			//	printf ("text/suppress mismatch %d [%d '%.*s'] %d %d\n",
			//			text_view->bol_suppress, to->size, to->size,
			//			passage->text + to->text, i, j);
			//}
			if (is_space) {
				if (!check_non_space (passage->text, to)) {
					continue;
				}
			} else {
				if (!check_space_or_nl (passage->text, to)) {
					continue;
				}
			}
			ret = 1;
			printf ("mixed space/text/\\n [%d '%.*s'] %d %d\n",
					to->size, to->size, passage->text + to->text, i, j);
		}
	}
	Passage_Delete (passage);

	ECS_DelRegistry (psg_sys.reg);
	return ret;
}
