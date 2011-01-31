
#include "clayland-private.h"

G_DEFINE_TYPE (ClaylandSurface, clayland_surface, CLUTTER_TYPE_TEXTURE);

static void
clayland_surface_dispose (GObject *object)
{
	ClaylandSurface *csurface = CLAYLAND_SURFACE(object);

	g_debug("disposing surface %p of type '%s'", object,
	        G_OBJECT_TYPE_NAME(object));

	G_OBJECT_CLASS (clayland_surface_parent_class)->dispose (object);

	if (csurface->compositor == NULL)
		return;
	g_object_unref(csurface->compositor);
	csurface->compositor = NULL;
}

static void
clayland_surface_finalize (GObject *object)
{
	ClaylandSurface *csurface = CLAYLAND_SURFACE(object);

	g_debug("finalizing surface %p of type '%s'", object,
	        G_OBJECT_TYPE_NAME(object));

	G_OBJECT_CLASS (clayland_surface_parent_class)->finalize (object);
	/* The above call has to come first so that the ClutterTexture class
	   releases its ref on the CoglHandle, making sure the last ref on the
	   CoglHandle is always held by the ClaylandBuffer. */
	if (csurface->buffer)
		g_object_unref(csurface->buffer);
}

static void
clayland_surface_class_init (ClaylandSurfaceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose  = clayland_surface_dispose;
	object_class->finalize = clayland_surface_finalize;
}

static void
clayland_surface_init (ClaylandSurface *surface)
{
	surface->buffer = NULL;
	surface->compositor = NULL;
	surface->fullscreen = FALSE;
}

static void
surface_destroy(struct wl_client *client,
		struct wl_surface *surface)
{
	wl_resource_destroy(&surface->resource, client);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, struct wl_buffer *buffer,
	       int32_t dx, int32_t dy)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandBuffer *cbuffer =
		container_of(buffer, ClaylandBuffer, buffer);
	gfloat x, y;

	clutter_actor_get_position (CLUTTER_ACTOR (csurface), &x, &y);

	clutter_texture_set_cogl_texture(&csurface->texture,
	                                 cbuffer->tex_handle);
	clutter_actor_set_position (CLUTTER_ACTOR(csurface),
				    x + dx, y + dy);
	clutter_actor_set_size (CLUTTER_ACTOR(csurface),
	                        buffer->width, buffer->height);

	buffer->attach(buffer, surface); /* XXX: does nothing right now */

	if (csurface->buffer != NULL) {
		g_debug("detaching buffer %p from surface %p",
		        csurface->buffer, csurface);
		g_object_unref(csurface->buffer);
	}
	csurface->buffer = g_object_ref(cbuffer);

	g_debug("attaching buffer %p to surface %p", cbuffer, csurface);
}

static void
surface_map_toplevel(struct wl_client *client,
	    struct wl_surface *surface)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);

	if (csurface->fullscreen) {
		clutter_actor_set_size(CLUTTER_ACTOR(csurface),
			csurface->width, csurface->height);
		clutter_actor_set_position(CLUTTER_ACTOR(csurface),
			csurface->x, csurface->y);
		csurface->fullscreen = FALSE;
	}

	clutter_actor_show (CLUTTER_ACTOR(csurface));
	clutter_actor_set_reactive (CLUTTER_ACTOR (csurface), TRUE);
}

static void
surface_map_transient(struct wl_client *client,
	    struct wl_surface *surface, struct wl_surface *parent,
	    int32_t dx, int32_t dy, uint32_t flags)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandSurface *cparent =
		container_of(parent, ClaylandSurface, surface);
	gfloat x, y;

	if (csurface->fullscreen) {
		clutter_actor_set_size(CLUTTER_ACTOR(csurface),
			csurface->width, csurface->height);
		csurface->fullscreen = FALSE;
	}

	clutter_actor_get_position (CLUTTER_ACTOR (cparent), &x, &y);
	clutter_actor_set_position (CLUTTER_ACTOR (csurface),
				    x + dx, y + dy);

	clutter_actor_show (CLUTTER_ACTOR (csurface));
	clutter_actor_set_reactive (CLUTTER_ACTOR (csurface), TRUE);
}

static void
surface_map_fullscreen(struct wl_client *client,
	    struct wl_surface *surface)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClutterActor *container = csurface->compositor->container;
	gfloat width, height, x, y;

	if (csurface->fullscreen)
		return;
	csurface->fullscreen = TRUE;

	clutter_actor_get_size(CLUTTER_ACTOR(csurface), &width, &height);
	clutter_actor_get_position(CLUTTER_ACTOR(csurface), &x, &y);
	csurface->width = width;
	csurface->height = height;
	csurface->x = x;
	csurface->y = y;

	/* XXX: The container might change size...
	        Is there a Clutter API for mapping one actor to cover another? */
	clutter_actor_get_size(container, &width, &height);
	clutter_actor_set_size(CLUTTER_ACTOR(csurface), width, height);
	clutter_actor_set_position(CLUTTER_ACTOR(csurface), 0.0, 0.0);

	clutter_actor_show (CLUTTER_ACTOR(csurface));
	clutter_actor_set_reactive (CLUTTER_ACTOR (csurface), TRUE);
}

static void
surface_damage(struct wl_client *client,
	       struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandBuffer *cbuffer = csurface->buffer;

	if (cbuffer != NULL)
		cbuffer->buffer.damage(&cbuffer->buffer, surface,
		                       x, y, width, height);
	/* damage event: TODO */
}

const struct wl_surface_interface clayland_surface_interface = {
	surface_destroy,
	surface_attach,
	surface_map_toplevel,
	surface_map_transient,
	surface_map_fullscreen,
	surface_damage
};

