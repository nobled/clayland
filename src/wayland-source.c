/* Clayland.
 * A library for building Wayland compositors.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <clutter/clutter.h>
#include <glib.h>
#include <wayland-server.h>

#include "clayland.h"

typedef struct _WlSource {
	GSource source;
	GPollFD pfd;
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

	/* The various callbacks will call into the Clutter API,
	   so we need to hold the global lock. */
	clutter_threads_enter();
	wl_event_loop_dispatch(source->loop, 0);
	clutter_threads_leave();

	return TRUE;
}

static GSourceFuncs wl_glib_source_funcs = {
	wl_glib_source_prepare,
	wl_glib_source_check,
	wl_glib_source_dispatch,
	NULL
};

GSource *
clayland_source_new(struct wl_event_loop *loop)
{
	WlSource *source;

	g_return_val_if_fail(loop != NULL, NULL);

	source = (WlSource *) g_source_new(&wl_glib_source_funcs,
					   sizeof (WlSource));
	if (source == NULL)
		return NULL;

	source->loop = loop;
	source->pfd.fd = wl_event_loop_get_fd(loop);
	source->pfd.events = G_IO_IN | G_IO_ERR;
	g_source_add_poll(&source->source, &source->pfd);

	return &source->source;
}
