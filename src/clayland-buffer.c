
#include <clutter/clutter.h>
#if defined(CLUTTER_WINDOWING_EGL)
#include <clutter/egl/clutter-egl.h>
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <string.h>

#include "clayland-private.h"

G_DEFINE_ABSTRACT_TYPE (ClaylandBuffer, clayland_buffer, G_TYPE_OBJECT);

static void
clayland_buffer_finalize (GObject *object)
{
	ClaylandBuffer *cbuffer = CLAYLAND_BUFFER(object);

	g_debug("finalizing buffer %p of type '%s'", object,
	        G_OBJECT_TYPE_NAME(object));

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

	g_debug("destroying buffer %p of type '%s'", buffer,
	        G_OBJECT_TYPE_NAME(buffer));

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

static enum visual
enum_from_visual(ClaylandCompositor *compositor, struct wl_visual *visual)
{
	if (visual == &compositor->compositor.premultiplied_argb_visual)
		return VISUAL_ARGB_PRE;
	if (visual == &compositor->compositor.argb_visual)
		return VISUAL_ARGB;
	if (visual == &compositor->compositor.rgb_visual)
		return VISUAL_RGB;

	/* unknown visual. */
	return VISUAL_UNKNOWN;

}

static const gchar *
str_from_visual(enum visual evisual)
{
switch (evisual) {
	case VISUAL_ARGB_PRE:
		return "premultiplied ARGB";
	case VISUAL_ARGB:
		return "ARGB";
	case VISUAL_RGB:
		return "RGB";
	default:
		return "<unknown visual>";
	}
}

static CoglPixelFormat
format_from_visual(enum visual evisual)
{
switch (evisual) {
#if G_BYTE_ORDER == G_BIG_ENDIAN
	case VISUAL_ARGB_PRE:
		return COGL_PIXEL_FORMAT_ARGB_8888_PRE;
	case VISUAL_ARGB:
		return COGL_PIXEL_FORMAT_ARGB_8888;
	case VISUAL_RGB:
		return COGL_PIXEL_FORMAT_RGB_888;
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
	case VISUAL_ARGB_PRE:
		return COGL_PIXEL_FORMAT_BGRA_8888_PRE;
	case VISUAL_ARGB:
		return COGL_PIXEL_FORMAT_BGRA_8888;
	case VISUAL_RGB:
		return COGL_PIXEL_FORMAT_BGR_888;
#endif
	default:
		/* unknown visual. */
		return COGL_PIXEL_FORMAT_ANY;
	}
}

CoglPixelFormat
_clayland_init_buffer(ClaylandBuffer *cbuffer,
                      ClaylandCompositor *compositor,
                      uint32_t id, int32_t width, int32_t height,
                      struct wl_visual *visual)
{
	enum visual evisual;

	g_return_val_if_fail(width >= 0 && height >= 0,
	                     COGL_PIXEL_FORMAT_ANY);

	g_debug("creating buffer %p of type '%s'", cbuffer,
	        G_OBJECT_TYPE_NAME(cbuffer));

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

	evisual = enum_from_visual(compositor, visual);

	if (0)
	  g_debug("creating buffer %p (id=%u, width=%d, height=%d,"
	        " visual='%s')", cbuffer, id, width, height,
	        str_from_visual(evisual));

	return format_from_visual(evisual);
}


static gboolean
has_extension(const char *extensions, size_t target_len, const char *target)
{
	const char *ext, *next;
	size_t len;

	ext = extensions;
	while (ext) {
		next = strchr(ext, ' ');
		if (next) {
			len = (size_t)(next - ext);
			next++; /* point to the next extension string */
			if (len != target_len)
				goto no_match;
		} else {
		/* if 'ext' is the last extension string,
		   include the target's null terminator in the comparison */
			len = target_len+1;
		}

		if (strncmp(target, ext, len) == 0)
			break;
no_match:
		ext = next;
	};

        if (ext)
                g_debug("  found extension: %s", target);
        else
                g_debug("missing extension: %s", target);

	return ext != NULL;
}

static void
post_drm_device(struct wl_client *client, struct wl_object *global)
{
	ClaylandCompositor *compositor =
	    container_of(global, ClaylandCompositor, drm_object);

	g_debug("Advertising DRM device: %s", compositor->drm_path);

	wl_client_post_event(client, global, WL_DRM_DEVICE,
	                     compositor->drm_path);
}

void
_clayland_add_buffer_interfaces(ClaylandCompositor *compositor)
{
	EGLDisplay edpy;
	const char *extensions, *glextensions;
	gboolean drm;

	compositor->shm_object.interface = &wl_shm_interface;
	compositor->shm_object.implementation =
	    (void (**)(void)) &clayland_shm_interface;
	wl_display_add_object(compositor->display, &compositor->shm_object);
	wl_display_add_global(compositor->display, &compositor->shm_object, NULL);

	/* Can we figure out whether we're compiling against clutter
	 * x11 or not? */
	if (dri2_connect(compositor) < 0) {
		g_warning("DRI2 connect failed, disabling DRM buffers");
		return;
	}
#if defined(CLUTTER_WINDOWING_X11) && defined(CLUTTER_WINDOWING_EGL)
	edpy = clutter_egl_display ();
#else
	edpy = EGL_NO_DISPLAY;
#endif
	extensions = eglQueryString(edpy, EGL_EXTENSIONS);
	glextensions = glGetString(GL_EXTENSIONS);
	drm = has_extension(extensions, 18, "EGL_MESA_drm_image");
	drm = has_extension(extensions, 18, "EGL_KHR_image_base") && drm;
	drm = has_extension(glextensions, 16, "GL_OES_EGL_image") && drm;

	if (drm == FALSE)
		return;

	compositor->create_image =
	    (PFNEGLCREATEIMAGEKHRPROC)
	        eglGetProcAddress("eglCreateImageKHR");
	compositor->destroy_image =
	    (PFNEGLDESTROYIMAGEKHRPROC)
	        eglGetProcAddress("eglDestroyImageKHR");
	compositor->image2tex =
	    (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
	        eglGetProcAddress("glEGLImageTargetTexture2DOES");

	if (!(compositor->create_image) || !(compositor->destroy_image)
	    || !(compositor->image2tex)) {
		g_warning("failed to get function pointers");
		return;
	}

	compositor->drm_object.interface = &wl_drm_interface;
	compositor->drm_object.implementation =
	    (void (**)(void)) &clayland_drm_interface;
	wl_display_add_object(compositor->display, &compositor->drm_object);
	wl_display_add_global(compositor->display, &compositor->drm_object,
	                      post_drm_device);

	compositor->egl_display = edpy;
	g_debug("egl display %p", edpy);
}

