/*
 * Copyright © 2010 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include "wayland-server.h"
#include "clayland.h"

typedef struct _WlSource {
	GSource source;
	GPollFD pfd;
	uint32_t mask;
	struct wl_event_loop *loop;
} WlSource;

static gboolean
wl_glib_source_prepare(GSource *base, gint *timeout)
{
	WlSource *source = (WlSource *) base;

	*timeout = -1;

	return FALSE;
}

static gboolean
wl_glib_source_check(GSource *base)
{
	WlSource *source = (WlSource *) base;

	return source->pfd.revents;
}

static gboolean
wl_glib_source_dispatch(GSource *base,
			GSourceFunc callback,
			gpointer data)
{
	WlSource *source = (WlSource *) base;

	wl_event_loop_dispatch(source->loop, 0);

	return TRUE;
}

static GSourceFuncs wl_glib_source_funcs = {
	wl_glib_source_prepare,
	wl_glib_source_check,
	wl_glib_source_dispatch,
	NULL
};

static int
wl_glib_source_update(uint32_t mask, void *data)
{
	WlSource *source = data;

	source->mask = mask;

	return 0;
}

GSource *
wl_glib_source_new(struct wl_event_loop *loop)
{
	WlSource *source;

	source = (WlSource *) g_source_new(&wl_glib_source_funcs,
					   sizeof (WlSource));
	source->loop = loop;
	source->pfd.fd = wl_event_loop_get_fd(loop);
	source->pfd.events = G_IO_IN | G_IO_ERR;
	g_source_add_poll(&source->source, &source->pfd);

	return &source->source;
}
