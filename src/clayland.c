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

#include <wayland-server.h>
#include <clutter/clutter.h>

#include <sys/time.h>
#include <math.h>
#include <errno.h>
#include <glib.h>

#include "clayland-private.h"

G_DEFINE_TYPE (ClaylandCompositor, clayland_compositor, G_TYPE_OBJECT);

static void
clayland_compositor_finalize (GObject *object)
{
	ClaylandCompositor *compositor = CLAYLAND_COMPOSITOR(object);

	g_debug("finalizing compositor %p of type '%s'", object,
	        G_OBJECT_TYPE_NAME(object));

	if (compositor->output != NULL)
		g_object_unref(compositor->output);
	if (compositor->display != NULL)
		wl_display_destroy (compositor->display);

	G_OBJECT_CLASS (clayland_compositor_parent_class)->finalize (object);
}

static void
clayland_compositor_add_interfaces (ClaylandCompositor *compositor);

static void
clayland_compositor_class_init (ClaylandCompositorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = clayland_compositor_finalize;

	klass->add_interfaces = clayland_compositor_add_interfaces;
}

static void
clayland_compositor_init (ClaylandCompositor *compositor)
{
	compositor->display = NULL;
	compositor->output = NULL;
}


static uint32_t
get_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
destroy_surface(struct wl_resource *resource, struct wl_client *client)
{
	ClaylandSurface *surface =
		container_of(resource, ClaylandSurface, surface.resource);
	ClaylandOutput *output = surface->compositor->output;
	ClutterActor *container = _clayland_output_get_container(output);
	struct wl_listener *l, *next;
	uint32_t time;

	g_debug("destroying surface %p of type '%s'", surface,
	        G_OBJECT_TYPE_NAME(surface));

	time = get_time();
	wl_list_for_each_safe(l, next,
			      &surface->surface.destroy_listener_list, link)
		l->func(l, &surface->surface, time);

	clutter_container_remove_actor (CLUTTER_CONTAINER (container),
					CLUTTER_ACTOR (surface));
	g_object_unref(surface);
}

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_compositor *compositor, uint32_t id)
{
	ClaylandCompositor *clayland =
		container_of(compositor, ClaylandCompositor, compositor);
	ClutterActor *container;
	ClaylandSurface *surface;

	if (clayland->output == NULL)
		return;
	container = _clayland_output_get_container(clayland->output);

	surface = g_object_new (clayland_surface_get_type(), NULL);

	g_debug("creating surface %p of type '%s'", surface,
	        G_OBJECT_TYPE_NAME(surface));

	surface->compositor = g_object_ref (clayland);
	clutter_container_add_actor(CLUTTER_CONTAINER (container),
				    CLUTTER_ACTOR (surface));

	wl_list_init(&surface->surface.destroy_listener_list);
	surface->surface.resource.destroy = destroy_surface;

	surface->surface.resource.object.id = id;
	surface->surface.resource.object.interface = &wl_surface_interface;
	surface->surface.resource.object.implementation =
		(void (**)(void)) &clayland_surface_interface;
	surface->surface.client = client;

	/* Clutter actors inherit from GInitiallyUnowned, so the
	 * container now has the only reference.  We need to take one
	 * for the wayland resource as well. */
	g_object_ref (surface);
	wl_client_add_resource(client, &surface->surface.resource);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
};

struct wl_display *
clayland_compositor_get_display(ClaylandCompositor *compositor)
{
	g_return_val_if_fail(CLAYLAND_IS_COMPOSITOR(compositor), NULL);

	return compositor->display;
}

static void
_clayland_add_buffer_interfaces (ClaylandCompositor *compositor)
{
	ClaylandCompositorClass *klass;

	klass = CLAYLAND_COMPOSITOR_GET_CLASS(compositor);

	klass->add_interfaces(compositor);
}

ClaylandCompositor *
clayland_compositor_create(struct wl_display *display)
{
	ClaylandCompositor *compositor;

	g_return_val_if_fail(display != NULL, NULL);

	compositor = g_object_new (clayland_drm_compositor_get_type(), NULL);

	g_debug("creating compositor %p of type '%s'", compositor,
	        G_OBJECT_TYPE_NAME(compositor));

	compositor->display = display;

	if (wl_compositor_init(&compositor->compositor,
			       &compositor_interface,
			       compositor->display) < 0) {
		g_object_unref(compositor);
		return NULL;
	}

	_clayland_add_devices(compositor);
	_clayland_add_buffer_interfaces(compositor);

	compositor->shell.object.interface = &wl_shell_interface;
	compositor->shell.object.implementation =
		(void (**)(void)) &clayland_shell_interface;
	wl_display_add_object(compositor->display, &compositor->shell.object);
	if (wl_display_add_global(compositor->display,
				  &compositor->shell.object, NULL)) {
		g_object_unref(compositor);
		return NULL;
	}

	return compositor;
}

static void
clayland_compositor_add_interfaces (ClaylandCompositor *compositor)
{
	struct wl_display *display = compositor->display;
	struct wl_object *shm = &compositor->shm_object;

	shm->interface = &wl_shm_interface;
	shm->implementation = (void (**)(void)) &clayland_shm_interface;
	wl_display_add_object(display, shm);

	if (wl_display_add_global(display, shm, NULL))
		g_warning("failed to add shm global object");
}

