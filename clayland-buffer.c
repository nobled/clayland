
#include "clayland-private.h"

G_DEFINE_TYPE (ClaylandBuffer, clayland_buffer, G_TYPE_OBJECT);

static void
clayland_buffer_finalize (GObject *object)
{
	ClaylandBuffer *cbuffer = CLAYLAND_BUFFER(object);

	if (cbuffer->tex_handle != COGL_INVALID_HANDLE)
		cogl_handle_unref(cbuffer->tex_handle);
	G_OBJECT_CLASS (clayland_buffer_parent_class)->finalize (object);
}

static void
clayland_buffer_class_init (ClaylandBufferClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = clayland_buffer_finalize;
}

static void
clayland_buffer_init (ClaylandBuffer *buffer)
{
	buffer->tex_handle = COGL_INVALID_HANDLE;
}

static void
default_buffer_attach(struct wl_buffer *buffer, struct wl_surface *surface)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandBuffer *cbuffer =
		container_of(buffer, ClaylandBuffer, buffer);
	/* do nothing; surface_attach (below) has this covered. */
}

static void
default_buffer_damage(struct wl_buffer *buffer,
                      struct wl_surface *surface,
                      int32_t x, int32_t y, int32_t width, int32_t height)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandBuffer *cbuffer =
		container_of(buffer, ClaylandBuffer, buffer);

	/* damage event: TODO */
}

static void
resource_destroy_buffer(struct wl_resource *resource, struct wl_client *client)
{
	ClaylandBuffer *buffer =
		container_of(resource, ClaylandBuffer, buffer.resource);

	/* Note: Any number of surfaces might still hold a reference to this buffer.
	   See the _finalize methods for ClaylandBuffer (above) and its subclasses
	   (in clayland-drm.c and clayland-shm.c). */
	g_object_unref(buffer);
}

static void
client_destroy_buffer(struct wl_client *client, struct wl_buffer *buffer)
{
	/* indirectly calls resource_destroy_buffer() */
	wl_resource_destroy(&buffer->resource, client);
}

static const struct wl_buffer_interface default_buffer_interface = {
	client_destroy_buffer
};

CoglPixelFormat
_clayland_init_buffer(ClaylandBuffer *cbuffer,
                      ClaylandCompositor *compositor,
                      uint32_t id, int32_t width, int32_t height,
                      struct wl_visual *visual)
{
	g_return_val_if_fail(width >= 0 && height >= 0,
	                     COGL_PIXEL_FORMAT_ANY);

	cbuffer->buffer.compositor = &compositor->compositor;
	cbuffer->buffer.width = width;
	cbuffer->buffer.height = height;
	cbuffer->buffer.visual = visual;
	cbuffer->buffer.attach = default_buffer_attach;
	cbuffer->buffer.damage = default_buffer_damage;

	cbuffer->buffer.resource.destroy = resource_destroy_buffer;
	cbuffer->buffer.resource.object.id = id;
	cbuffer->buffer.resource.object.interface = &wl_buffer_interface;
	cbuffer->buffer.resource.object.implementation = (void (**)(void))
		&default_buffer_interface;

	if (visual == &compositor->compositor.premultiplied_argb_visual)
		return COGL_PIXEL_FORMAT_BGRA_8888_PRE;
	if (visual == &compositor->compositor.argb_visual)
		return COGL_PIXEL_FORMAT_BGRA_8888;
	if (visual == &compositor->compositor.rgb_visual)
		return COGL_PIXEL_FORMAT_BGR_888;

	/* unknown visual. */
	return COGL_PIXEL_FORMAT_ANY;
}

