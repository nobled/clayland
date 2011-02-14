#ifndef CLAYLAND_H
#define CLAYLAND_H

#include <clutter/clutter.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CLAYLAND_TYPE_COMPOSITOR            (clayland_compositor_get_type ())
#define CLAYLAND_COMPOSITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_COMPOSITOR, ClaylandCompositor))
#define CLAYLAND_IS_COMPOSITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_COMPOSITOR))

#if defined(__GNUC__) && __GNUC__ >= 4
#define CLAYLAND_EXPORT __attribute__((__visibility__("default")))
#else
#define CLAYLAND_EXPORT
#endif

struct wl_display;

typedef struct _ClaylandCompositor ClaylandCompositor;

CLAYLAND_EXPORT
GType clayland_compositor_get_type(void);

CLAYLAND_EXPORT
ClaylandCompositor *clayland_compositor_create(struct wl_display *display);

/* XXX: can only have one output for now */
CLAYLAND_EXPORT
void clayland_compositor_add_output(ClaylandCompositor *compositor,
                                    ClutterContainer *container);

CLAYLAND_EXPORT struct wl_display *
clayland_compositor_get_display(ClaylandCompositor *compositor);

G_END_DECLS

#endif /* CLAYLAND_H */
