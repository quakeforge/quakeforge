#ifndef __options_util_h
#define __options_util_h

@extern void opt_cursor (int x, int y);
@extern void draw_item (int x, int y, int spacing, string spacechar, string label, string valstr);
@extern void  draw_val_item (int x, int y, int spacing, string label, string valstr);
@extern int to_percentage (float min, float max, float val);
@extern float min_max_cnt (float min, float max, float step, float val, int cntflag);

#endif//__options_util_h
