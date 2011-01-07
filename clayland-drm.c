
#include <cogl/cogl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>

#include "clayland.h"

#define CLAYLAND_TYPE_DRM_BUFFER            (clayland_drm_buffer_get_type ())
#define CLAYLAND_DRM_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_DRM_BUFFER, ClaylandDrmBuffer))
#define CLAYLAND_DRM_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_DRM_BUFFER, ClaylandDrmBufferClass))
#define CLAYLAND_IS_DRM_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_DRM_BUFFER))
#define CLAYLAND_IS_DRM_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_DRM_BUFFER))
#define CLAYLAND_DRM_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_DRM_BUFFER, ClaylandDrmBufferClass))

typedef struct _ClaylandDrmBuffer ClaylandDrmBuffer;
typedef struct _ClaylandDrmBufferClass ClaylandDrmBufferClass;

struct _ClaylandDrmBuffer {
	ClaylandBuffer		 cbuffer;
	GLuint			 gltex_handle;
};

struct _ClaylandDrmBufferClass {
	ClaylandBufferClass	 cbuffer_class;
};

G_DEFINE_TYPE (ClaylandDrmBuffer, clayland_drm_buffer, CLAYLAND_TYPE_BUFFER);

static void
clayland_drm_buffer_finalize (GObject *object)
{
	ClaylandDrmBuffer *buffer = CLAYLAND_DRM_BUFFER(object);

	G_OBJECT_CLASS (clayland_drm_buffer_parent_class)->finalize (object);
	/* Cogl doesn't delete foreign texture handles itself on destruction,
	   so we need to delete this handle ourselves - *after* the above call
	   deletes the final reference to the CoglHandle. */
	if (buffer->gltex_handle != 0)
		glDeleteTextures(1, &buffer->gltex_handle);
}

static void
clayland_drm_buffer_class_init (ClaylandDrmBufferClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = clayland_drm_buffer_finalize;
}

static void
clayland_drm_buffer_init (ClaylandDrmBuffer *buffer)
{

}

static void
drm_buffer_destroy(struct wl_resource *resource, struct wl_client *client)
{
	ClaylandDrmBuffer *buffer =
		container_of(resource, ClaylandDrmBuffer, cbuffer.buffer.resource);

	/* note, any number of surfaces might still hold a reference to this buffer */
	g_object_unref(buffer);
}

static void
drm_buffer_create(struct wl_client *client, struct wl_drm *drm,
                  uint32_t id, uint32_t name, int32_t width, int32_t height,
                  uint32_t stride, struct wl_visual *visual)
{
	ClaylandCompositor *compositor =
	    container_of((struct wl_object *)drm, ClaylandCompositor, drm_object);
	ClaylandDrmBuffer *buffer;
	EGLImageKHR image;
	EGLint attribs[] = {
		EGL_WIDTH,  0,
		EGL_HEIGHT, 0,
		EGL_DRM_BUFFER_STRIDE_MESA, 0,
		EGL_DRM_BUFFER_FORMAT_MESA, EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_NONE
	};
	CoglPixelFormat pformat;

	buffer = g_object_new(CLAYLAND_TYPE_DRM_BUFFER, NULL);
	if (buffer == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	pformat = _clayland_init_buffer(&buffer->cbuffer, compositor,
	                                id, width, height, visual);
	if (pformat == COGL_PIXEL_FORMAT_ANY) {
		/* XXX: report error? */
		g_object_unref(buffer);
		return;
	}

	buffer->cbuffer.buffer.resource.destroy = drm_buffer_destroy;

	attribs[1] = width;
	attribs[3] = height;
	attribs[5] = stride / 4; /* convert from bytes to pixels */

	image = compositor->create_image(compositor->egl_display,
	                          EGL_NO_CONTEXT,
	                          EGL_DRM_BUFFER_MESA,
	                          (EGLClientBuffer)name, attribs);
	if (image == NULL) {
		fprintf(stderr, "failed to create EGLImage\n");
		/* XXX: report error? */
		g_object_unref(buffer);
		return;
	}

	glGenTextures(1, &buffer->gltex_handle);
	glBindTexture(GL_TEXTURE_2D, buffer->gltex_handle);
	compositor->image2tex(GL_TEXTURE_2D, image);
	compositor->destroy_image(compositor->egl_display, image);

	buffer->cbuffer.tex_handle =
	    cogl_texture_new_from_foreign(buffer->gltex_handle, GL_TEXTURE_2D,
	                (GLuint)width, (GLuint)height, 0, 0, pformat);

	if (buffer->cbuffer.tex_handle == COGL_INVALID_HANDLE) {
		fprintf(stderr, "failed to create CoglHandle\n");
		g_object_unref(buffer);
		return;
	}

	wl_client_add_resource(client, &buffer->cbuffer.buffer.resource);
}

static void
drm_authenticate(struct wl_client *client, struct wl_drm *drm, uint32_t magic)
{
	ClaylandCompositor *compositor =
	    container_of((struct wl_object *)drm, ClaylandCompositor, drm_object);

	if (dri2_authenticate(magic) < 0) {
		wl_client_post_event(client,
				(struct wl_object *)compositor->display,
				WL_DISPLAY_INVALID_OBJECT, 0);
		return;
	}

	wl_client_post_event(client, &compositor->drm_object,
	                        WL_DRM_AUTHENTICATED);
}


const struct wl_drm_interface clayland_drm_interface = {
	drm_authenticate,
	drm_buffer_create
};

