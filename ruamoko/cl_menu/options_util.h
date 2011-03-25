#ifndef __options_util_h
#define __options_util_h

@extern void (int x, int y) opt_cursor;
@extern void (int x, int y, int spacing, string spacechar, string label, string valstr) draw_item;
@extern void (int x, int y, int spacing, string label, string valstr) draw_val_item;
@extern int (float min, float max, float val) to_percentage;
@extern float (float min, float max, float step, float val, int cntflag) min_max_cnt;

#endif//__options_util_h
