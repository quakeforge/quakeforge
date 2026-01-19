/*
	vid_wl_sw.c

	Software Wayland video driver (8/32 bit)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  contributors of the QuakeForge project
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/ui/view.h"

#include "context_wl.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

#include <wayland-client.h>

typedef union {
	byte        bgra[4];
	uint32_t    value;
} wl_palette_t;

static wl_palette_t st2d_8to32table[256];
static byte current_palette[768];
static bool palette_changed;

typedef struct wl_buffer_data_s {
    struct wl_buffer *buf;
    uint32_t *buf_data;
    size_t size;
    size_t stride;
} wl_buffer_data_t;

static wl_buffer_data_t* buffer_data;

static void
wl_set_palette (sw_ctx_t *ctx, const byte *palette)
{
    palette_changed = true;

    if (palette != current_palette) {
        memcpy (current_palette, palette, sizeof (current_palette));
    }

	for (int i = 0; i < 256; i++) {
		const byte *pal = palette + 3 * i;
		st2d_8to32table[i].bgra[0] = viddef.gammatable[pal[2]];
		st2d_8to32table[i].bgra[1] = viddef.gammatable[pal[1]];
		st2d_8to32table[i].bgra[2] = viddef.gammatable[pal[0]];
		st2d_8to32table[i].bgra[3] = 255;
	}
}

static void
wl_commit_surface (void)
{
    if (wl_surface_configured) {
        wl_surface_attach (wl_surface, buffer_data->buf, 0, 0);
        wl_surface_damage_buffer (wl_surface, 0, 0, INT32_MAX, INT32_MAX);
        wl_surface_commit (wl_surface);
    }
}

static void
wl_blit_rect (sw_ctx_t *ctx, vrect_t *rect)
{
    sw_framebuffer_t *fb = ctx->framebuffer->buffer;

    auto src = fb->color + rect->y * fb->rowbytes + rect->x;
    auto dst = buffer_data->buf_data;

    for (int y = rect->height; y-- > 0; ) {
        for (int x = rect->width; x-- > 0; ) {
            *dst++ = st2d_8to32table[*src++].value;
        }

        src += fb->rowbytes - rect->width;
        dst += buffer_data->stride - rect->width;
    }

    wl_commit_surface ();
}

static void
wl_sw8_8_update (sw_ctx_t *ctx, vrect_t *rects)
{
    vrect_t full_rect;
    if (palette_changed) {
        palette_changed = false;
        full_rect.x = 0;
        full_rect.y = 0;
        full_rect.width = viddef.width;
        full_rect.height = viddef.height;
        full_rect.next = nullptr;
        rects = &full_rect;
    }

    while (rects) {
        wl_blit_rect (ctx, rects);
        rects = rects->next;
    }

    wl_commit_surface ();
}

static sw_framebuffer_t swfb;
static framebuffer_t fb = { .buffer = &swfb };

static void
shm_name (char *buf)
{
    struct timespec ts;
	clock_gettime (CLOCK_REALTIME, &ts);
	auto r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A' + (r & 15) + (r & 16) * 2;
		r >>= 5;
	}
}

static int
create_shm_file (void)
{
    int retries = 100;
    do {
        char name[] = "/wl_shm-XXXXXX";
        shm_name (name + sizeof(name) - 7);
        
        retries--;

        auto fd = shm_open (name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink (name);
            return fd;
        }

    } while (retries > 0 && errno == EEXIST);

    return -1;
}

static int
allocate_shm_file (size_t size)
{
    auto fd = create_shm_file ();
    if (fd < 0)
        Sys_Error ("VID: create_shm_file failed");

    int ret;
    do {
        ret = ftruncate (fd, size);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0)
        Sys_Error ("VID: allocate_shm_file failed to truncate file");

    return fd;
}

static void
wl_release_buffer (void *data, struct wl_buffer *buf)
{
    Sys_MaskPrintf (SYS_wayland, "Wayland: releasing buffer\n");

    wl_buffer_destroy (buf);

    wl_buffer_data_t* bdata = data;
    munmap (bdata->buf_data, bdata->size);
    free (bdata);
}

static const struct wl_buffer_listener wl_buf_listener = {
    .release = wl_release_buffer
};

static void
wl_allocate_framebuffer (void)
{
    auto stride = viddef.width * 4;
    auto size = viddef.height * stride;
    auto fd = allocate_shm_file (size);
    byte *data = mmap (nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (data == MAP_FAILED) {
        close (fd);
        Sys_Error ("wl_init_buffers: Failed to map pool memory");
    }

    buffer_data = calloc (1, sizeof (wl_buffer_data_t));
    buffer_data->buf_data = (uint32_t *) data;
    buffer_data->size = size;
    buffer_data->stride = stride / 4;

    auto pool = wl_shm_create_pool (wl_shm, fd, size);
    buffer_data->buf = wl_shm_pool_create_buffer (pool, 0, viddef.width, viddef.height,
                                                stride, WL_SHM_FORMAT_XRGB8888);
    wl_buffer_add_listener (buffer_data->buf, &wl_buf_listener, buffer_data);
    wl_shm_pool_destroy (pool);
    close (fd);
}

static void
wl_init_buffers (void *data)
{
    sw_ctx_t *ctx = data;

    ctx->framebuffer = &fb;

    wl_allocate_framebuffer ();

    fb.width = viddef.width;
    fb.height = viddef.height;

    if (swfb.color) {
        free (swfb.color);
    }

    swfb.rowbytes = viddef.width;
    swfb.color = calloc (swfb.rowbytes, viddef.height);
    swfb.depth = 0;
}

static void
wl_create_context (sw_ctx_t *ctx)
{
    viddef.vid_internal->init_buffers = wl_init_buffers;
}

static void
wl_choose_visual (sw_ctx_t *ctx)
{
}

sw_ctx_t *
WL_SW_Context (vid_internal_t *vi)
{
    sw_ctx_t *ctx = calloc (1, sizeof (sw_ctx_t));
	ctx->set_palette = wl_set_palette;
	ctx->choose_visual = wl_choose_visual;
	ctx->create_context = wl_create_context;
	ctx->update = wl_sw8_8_update;

	vi->ctx = ctx;
	return ctx;
}

void
WL_SW_Init_Cvars (void)
{
}
