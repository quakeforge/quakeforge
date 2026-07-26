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

extern bool physics_paused;

extern int in_context;

#endif//__qwaq_ed_h
