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
	surface->constraints.x = NULL;
	surface->constraints.y = NULL;
	surface->constraints.w = NULL;
	surface->constraints.h = NULL;
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

static inline void
remove_constraint(ClutterActor *cs, ClutterConstraint **c)
{
	if (*c == NULL)
		return;
	clutter_actor_remove_constraint(cs, *c);
	g_object_unref(*c);
	*c = NULL;
}

static inline void
remove_constraints(ClaylandSurface *cs)
{
	remove_constraint(CLUTTER_ACTOR(cs), &cs->constraints.x);
	remove_constraint(CLUTTER_ACTOR(cs), &cs->constraints.y);
	remove_constraint(CLUTTER_ACTOR(cs), &cs->constraints.w);
	remove_constraint(CLUTTER_ACTOR(cs), &cs->constraints.h);
}

static void
surface_map_toplevel(struct wl_client *client,
	    struct wl_surface *surface)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);

	remove_constraints(csurface);

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
	ClutterConstraint *x, *y;

	if (SURFACE_IS_TRANSIENT(csurface) &&
	   (SURFACE_GET_TRANSIENT_PARENT(csurface) == CLUTTER_ACTOR(cparent)))
		return;

	remove_constraints(csurface);

	x = clutter_bind_constraint_new(CLUTTER_ACTOR(cparent),
	                                CLUTTER_BIND_X, (gfloat)dx);
	y = clutter_bind_constraint_new(CLUTTER_ACTOR(cparent),
	                                CLUTTER_BIND_Y, (gfloat)dy);

	if (x == NULL || y == NULL) {
		wl_client_post_no_memory(client);
		g_warning("surface.map_transient failed:"
		          " Could not create constraints");
		g_object_unref(x);
		g_object_unref(y);
		return;
	}
	csurface->constraints.x = g_object_ref(x);
	csurface->constraints.y = g_object_ref(y);

	clutter_actor_add_constraint(CLUTTER_ACTOR(csurface), x);
	clutter_actor_add_constraint(CLUTTER_ACTOR(csurface), y);

	clutter_actor_show (CLUTTER_ACTOR (csurface));
	clutter_actor_set_reactive (CLUTTER_ACTOR (csurface), TRUE);
}

static void
surface_map_fullscreen(struct wl_client *client,
	    struct wl_surface *surface)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandOutput *output = csurface->compositor->output;
	ClutterActor *container = _clayland_output_get_container(output);
	ClutterConstraint *x, *y, *w, *h;

	if (SURFACE_IS_FULLSCREEN(csurface))
		return;
	remove_constraints(csurface);

	/* XXX: This only works if the container uses a fixed
	        ClutterLayoutManager, like ClutterBox or ClutterStage do */
	x = clutter_bind_constraint_new(CLUTTER_ACTOR(container),
	                                CLUTTER_BIND_X, 0.0);
	y = clutter_bind_constraint_new(CLUTTER_ACTOR(container),
	                                CLUTTER_BIND_Y, 0.0);
	w = clutter_bind_constraint_new(CLUTTER_ACTOR(container),
	                                CLUTTER_BIND_WIDTH, 0.0);
	h = clutter_bind_constraint_new(CLUTTER_ACTOR(container),
	                                CLUTTER_BIND_HEIGHT, 0.0);

	if (x == NULL || y == NULL || w == NULL || h == NULL) {
		wl_client_post_no_memory(client);
		g_warning("surface.map_fullscreen failed:"
		          " Could not create constraints");
		g_object_unref(x);
		g_object_unref(y);
		g_object_unref(w);
		g_object_unref(h);
		return;
	}
	csurface->constraints.x = g_object_ref(x);
	csurface->constraints.y = g_object_ref(y);
	csurface->constraints.w = g_object_ref(w);
	csurface->constraints.h = g_object_ref(h);

	clutter_actor_add_constraint(CLUTTER_ACTOR(csurface), x);
	clutter_actor_add_constraint(CLUTTER_ACTOR(csurface), y);
	clutter_actor_add_constraint(CLUTTER_ACTOR(csurface), w);
	clutter_actor_add_constraint(CLUTTER_ACTOR(csurface), h);

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

