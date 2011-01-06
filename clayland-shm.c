
#include <sys/mman.h>
#include <unistd.h>

#include "clayland.h"

#define CLAYLAND_TYPE_SHM_BUFFER            (clayland_shm_buffer_get_type ())
#define CLAYLAND_SHM_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_SHM_BUFFER, ClaylandShmBuffer))
#define CLAYLAND_SHM_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_SHM_BUFFER, ClaylandShmBufferClass))
#define CLAYLAND_IS_SHM_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_SHM_BUFFER))
#define CLAYLAND_IS_SHM_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_SHM_BUFFER))
#define CLAYLAND_SHM_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_SHM_BUFFER, ClaylandShmBufferClass))

typedef struct _ClaylandShmBuffer ClaylandShmBuffer;
typedef struct _ClaylandShmBufferClass ClaylandShmBufferClass;

struct _ClaylandShmBuffer {
	ClaylandBuffer		 cbuffer;
	guint8			*data;
	size_t			 size;
};

struct _ClaylandShmBufferClass {
	ClaylandBufferClass	 cbuffer_class;
};

G_DEFINE_TYPE (ClaylandShmBuffer, clayland_shm_buffer, CLAYLAND_TYPE_BUFFER);

static void
clayland_shm_buffer_finalize (GObject *object)
{
	ClaylandShmBuffer *buffer = CLAYLAND_SHM_BUFFER(object);

	G_OBJECT_CLASS (clayland_shm_buffer_parent_class)->finalize (object);
	/* The CoglHandle is still holding onto the data, so don't unmap it
	   until *after* the above call deletes the final reference to
	   the CoglHandle. */
	if (buffer->data != MAP_FAILED)
		munmap(buffer->data, buffer->size);
}

static void
clayland_shm_buffer_class_init (ClaylandShmBufferClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = clayland_shm_buffer_finalize;
}

static void
clayland_shm_buffer_init (ClaylandShmBuffer *buffer)
{
	buffer->data = MAP_FAILED;
}

static void
shm_buffer_destroy(struct wl_resource *resource, struct wl_client *client)
{
	ClaylandShmBuffer *buffer =
		container_of(resource, ClaylandShmBuffer, cbuffer.buffer.resource);

	/* note, any number of surfaces might still hold a reference to this buffer */
	g_object_unref(buffer);
}

static void
shm_buffer_create(struct wl_client *client, struct wl_shm *shm,
		  uint32_t id, int fd, int32_t width, int32_t height,
		  uint32_t stride, struct wl_visual *visual)
{
	ClaylandCompositor *compositor =
	    container_of((struct wl_object *)shm, ClaylandCompositor, shm_object);
	ClaylandShmBuffer *buffer;
	CoglPixelFormat pformat;
	CoglTextureFlags flags = COGL_TEXTURE_NONE; /* XXX: tweak flags? */

	buffer = g_object_new(CLAYLAND_TYPE_SHM_BUFFER, NULL);
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

	buffer->cbuffer.buffer.resource.destroy = shm_buffer_destroy;

	buffer->size = stride * height;
	buffer->data = mmap(NULL, buffer->size,
	                    PROT_READ, MAP_SHARED, fd, 0);
	(void) close(fd);
	if (buffer->data == MAP_FAILED) {
		g_object_unref(buffer);
		return;
	}

	buffer->cbuffer.tex_handle =
	cogl_texture_new_from_data((unsigned int)width, (unsigned int)height,
	                flags, pformat, COGL_PIXEL_FORMAT_ANY, stride, buffer->data);

	if (buffer->cbuffer.tex_handle == COGL_INVALID_HANDLE) {
		/* XXX: move munmap into GObject destructor? */
		munmap(buffer->data, buffer->size);
		g_object_unref(buffer);
		return;
	}


	wl_client_add_resource(client, &buffer->cbuffer.buffer.resource);
}

const struct wl_shm_interface clayland_shm_interface = {
	shm_buffer_create
};

