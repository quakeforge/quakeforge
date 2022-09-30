#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>

#include "QF/ui/view.h"
#include "QF/ui/passage.h"

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

	passage_t  *passage = Passage_ParseText (test_text);
	if (passage->view->num_children != 3) {
		ret = 1;
		printf ("incorrect number of paragraphs: %d\n",
				passage->view->num_children);
	}
	if (passage->num_text_objects != 140) {
		ret = 1;
		printf ("incorrect number of text objects: %d\n",
				passage->num_text_objects);
	}
	if (passage->view->children[0]->num_children != 49) {
		ret = 1;
		printf ("incorrect number of text objects in first paragraph: %d\n",
				passage->view->children[0]->num_children);
	}
	if (passage->view->children[1]->num_children != 90) {
		ret = 1;
		printf ("incorrect number of text objects in second paragraph: %d\n",
				passage->view->children[1]->num_children);
	}
	if (passage->view->children[2]->num_children != 1) {
		ret = 1;
		printf ("incorrect number of text objects in third paragraph: %d\n",
				passage->view->children[1]->num_children);
	}
	view_t     *text_view = passage->view->children[1]->children[0];
	psg_text_t *to = text_view->data;
	if (to->size != 2 && (passage->text[to->text] != ' '
						  && passage->text[to->text + 1] != ' ')) {
		ret = 1;
		printf ("second paragram does not begin with double space: %d '%.*s'\n",
				to->size, to->size, passage->text + to->text);
	}
	if (text_view->bol_suppress) {
		ret = 1;
		printf ("second paragram indent suppressed\n");
	}
	for (int i = 0; i < passage->view->num_children; i++) {
		view_t     *paragraph_view = passage->view->children[i];
		for (int j = 0; j < paragraph_view->num_children; j++) {
			view_t     *text_view = paragraph_view->children[j];
			psg_text_t *to = text_view->data;
			unsigned    is_space = Passage_IsSpace (passage->text + to->text);
			if (i == 1 && j == 0) {
				// second paragraph indent, tested above
				continue;
			}
			if ((!!is_space) != text_view->bol_suppress) {
				ret = 1;
				printf ("text/suppress mismatch %d [%d '%.*s'] %d %d\n",
						text_view->bol_suppress, to->size, to->size,
						passage->text + to->text, i, j);
			}
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

	return ret;
}
