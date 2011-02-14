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

	if (compositor->source != NULL) {
		g_source_destroy(compositor->source);
		g_source_unref(compositor->source);
	}
	if (compositor->output != NULL)
		g_object_unref(compositor->output);
	if (compositor->display != NULL)
		wl_display_destroy (compositor->display);
	if (compositor->drm_fd >= 0)
		(void) close(compositor->drm_fd);
	if (compositor->drm_path != NULL)
		g_free(compositor->drm_path);

	G_OBJECT_CLASS (clayland_compositor_parent_class)->finalize (object);
}

static void
clayland_compositor_class_init (ClaylandCompositorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = clayland_compositor_finalize;
}

static void
clayland_compositor_init (ClaylandCompositor *compositor)
{
	compositor->display = NULL;
	compositor->source = NULL;
	compositor->output = NULL;
	compositor->drm_path = NULL;
	compositor->drm_fd = -1;
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
	ClutterActor *container = surface->compositor->output->container;
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
	container = clayland->output->container;

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

ClaylandCompositor *
clayland_compositor_create(struct wl_display *display)
{
	ClaylandCompositor *compositor;

	g_return_val_if_fail(display != NULL, NULL);

	compositor = g_object_new (clayland_compositor_get_type(), NULL);

	g_debug("creating compositor %p of type '%s'", compositor,
	        G_OBJECT_TYPE_NAME(compositor));

	compositor->display = display;

	if (wl_compositor_init(&compositor->compositor,
			       &compositor_interface,
			       compositor->display) < 0) {
		g_object_unref(compositor);
		return NULL;
	}

	compositor->loop = wl_display_get_event_loop(compositor->display);
	compositor->source = wl_glib_source_new(compositor->loop);
	g_source_attach(compositor->source, NULL);

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

