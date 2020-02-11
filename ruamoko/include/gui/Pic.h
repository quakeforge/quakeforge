#ifndef __ruamoko_gui_Pic_h
#define __ruamoko_gui_Pic_h

#include "gui/View.h"

/**	\addtogroup gui */
///@{

@interface Pic : View
{
	string pic_name;
}
-(void)setPic:(string)pic;
-(void)draw;
@end

@interface CenterPic : Pic
-(void)draw;
@end

///@}

#endif//__ruamoko_gui_Pic_h
