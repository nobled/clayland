#ifndef CLAYLAND_H
#define CLAYLAND_H

#include <clutter/clutter.h>
#include <glib.h>
#include <glib-object.h>

#define CLAYLAND_TYPE_COMPOSITOR            (clayland_compositor_get_type ())
#define CLAYLAND_COMPOSITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_COMPOSITOR, ClaylandCompositor))
#define CLAYLAND_COMPOSITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_COMPOSITOR, ClaylandCompositorClass))
#define CLAYLAND_IS_COMPOSITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_COMPOSITOR))
#define CLAYLAND_IS_COMPOSITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_COMPOSITOR))
#define CLAYLAND_COMPOSITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_COMPOSITOR, ClaylandCompositorClass))

typedef struct _ClaylandCompositor ClaylandCompositor;
typedef struct _ClaylandCompositorClass ClaylandCompositorClass;

GType clayland_compositor_get_type(void);

ClaylandCompositor *clayland_compositor_create(ClutterActor *stage);

#endif /* CLAYLAND_H */
