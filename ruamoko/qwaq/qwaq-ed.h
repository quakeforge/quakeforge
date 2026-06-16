#ifndef __qwaq_ed_h
#define __qwaq_ed_h

#include <input.h>
#include <msgbuf.h>

#include "pga3d.h"

void printf (string fmt, ...);

void Render_UpdateBuffer (string name, ulong offset, void *data, ulong size);
ulong Render_BufferAddress (string name);
ulong Render_BufferOffset (string name);
ulong Render_BufferSize (string name);

msgbuf_t create_ico();
msgbuf_t create_block(vec3 block_size);
msgbuf_t create_quadsphere(bool do_colors);
body_t calc_inertia_tensor (msgbuf_t model_buf, float inv_density);
void leafnode();

extern int in_context;
extern in_axis_t *cam_move_forward;
extern in_axis_t *cam_move_side;
extern in_axis_t *cam_move_up;
extern in_axis_t *cam_move_pitch;
extern in_axis_t *cam_move_yaw;
extern in_axis_t *cam_move_roll;
extern in_button_t *cam_next;
extern in_button_t *cam_prev;
extern in_axis_t *mouse_x;
extern in_axis_t *mouse_y;

extern in_axis_t *move_forward;
extern in_axis_t *move_side;
extern in_axis_t *move_up;
extern in_axis_t *move_pitch;
extern in_axis_t *move_yaw;
extern in_axis_t *move_roll;
extern in_axis_t *look_forward;
extern in_axis_t *look_right;
extern in_axis_t *look_up;
extern in_button_t *shift;
extern in_button_t *move_jump;
extern in_button_t *target_lock;

extern vec2 mouse_start;

#endif//__qwaq_ed_h
