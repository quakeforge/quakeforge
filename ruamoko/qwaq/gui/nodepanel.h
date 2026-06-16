#ifndef __qwaq_gui_nodepanel_h
#define __qwaq_gui_nodepanel_h

#include "window.h"

@interface NodePanel : Window
{
	Array *buttons;
	Array *axes;

	ListView *virtView;
}
+panel:(imui_ctx_t)ctx buttons:(Array*)buttons axes:(Array*)axes;
@end

#endif//__qwaq_gui_nodepanel_h
