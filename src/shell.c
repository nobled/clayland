
#include "clayland-private.h"

typedef struct _ClaylandMoveGrab {
	struct wl_grab grab;
	int32_t dx, dy;
} ClaylandMoveGrab;

static void
move_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	ClaylandMoveGrab *move = (ClaylandMoveGrab *) grab;
	ClaylandSurface *cs =
		container_of(grab->input_device->pointer_focus,
			     ClaylandSurface, surface);

	clutter_actor_set_position (CLUTTER_ACTOR (cs),
				    x + move->dx, y + move->dy);
}

static void
move_grab_button(struct wl_grab *grab,
		 uint32_t time, int32_t button, int32_t state)
{
}

static void
move_grab_end(struct wl_grab *grab, uint32_t time)
{
	g_free(grab);
}

static const struct wl_grab_interface move_grab_interface = {
	move_grab_motion,
	move_grab_button,
	move_grab_end
};

static void
shell_move(struct wl_client *client, struct wl_shell *shell,
	   struct wl_surface *surface,
	   struct wl_input_device *device, uint32_t time)
{
	ClaylandSurface *cs = container_of(surface, ClaylandSurface, surface);
	ClaylandMoveGrab *move;
	gfloat x, y;

	move = g_malloc(sizeof *move);
	if (!move) {
		wl_client_post_no_memory(client);
		return;
	}

	clutter_actor_get_position (CLUTTER_ACTOR (cs), &x, &y);

	move->grab.interface = &move_grab_interface;
	move->dx = x - device->grab_x;
	move->dy = y - device->grab_y;

	if (wl_input_device_update_grab(device,
					&move->grab, surface, time) < 0)
		return;
}

typedef struct _ClaylandResizeGrab {
	struct wl_grab grab;
	uint32_t edges;
	int32_t dx, dy, width, height;
} ClaylandResizeGrab;

static void
resize_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	ClaylandResizeGrab *resize = (ClaylandResizeGrab *) grab;
	struct wl_input_device *device = grab->input_device;
	ClaylandCompositor *compositor =
		container_of(device->compositor,
			     ClaylandCompositor, compositor);
	struct wl_surface *surface = device->pointer_focus;
	int32_t width, height;

	if (resize->edges & WL_SHELL_RESIZE_LEFT) {
		width = device->grab_x - x + resize->width;
	} else if (resize->edges & WL_SHELL_RESIZE_RIGHT) {
		width = x - device->grab_x + resize->width;
	} else {
		width = resize->width;
	}

	if (resize->edges & WL_SHELL_RESIZE_TOP) {
		height = device->grab_y - y + resize->height;
	} else if (resize->edges & WL_SHELL_RESIZE_BOTTOM) {
		height = y - device->grab_y + resize->height;
	} else {
		height = resize->height;
	}

	wl_client_post_event(surface->client, &compositor->shell.object,
			     WL_SHELL_CONFIGURE, time, resize->edges,
			     surface, width, height);
}

static void
resize_grab_button(struct wl_grab *grab,
		   uint32_t time, int32_t button, int32_t state)
{
}

static void
resize_grab_end(struct wl_grab *grab, uint32_t time)
{
	g_free(grab);
}

static const struct wl_grab_interface resize_grab_interface = {
	resize_grab_motion,
	resize_grab_button,
	resize_grab_end
};

static void
shell_resize(struct wl_client *client, struct wl_shell *shell,
	     struct wl_surface *surface,
	     struct wl_input_device *device, uint32_t time, uint32_t edges)
{
	ClaylandResizeGrab *resize;
	ClaylandSurface *cs = container_of(surface, ClaylandSurface, surface);
	gfloat x, y, width, height;

	resize = g_malloc(sizeof *resize);
	if (!resize) {
		wl_client_post_no_memory(client);
		return;
	}

	clutter_actor_get_position (CLUTTER_ACTOR (cs), &x, &y);
	clutter_actor_get_size (CLUTTER_ACTOR (cs), &width, &height);

	resize->grab.interface = &resize_grab_interface;
	resize->edges = edges;
	resize->dx = x - device->grab_x;
	resize->dy = y - device->grab_y;
	resize->width = width;
	resize->height = height;

	if (edges == 0 || edges > 15 ||
	    (edges & 3) == 3 || (edges & 12) == 12)
		return;

	if (wl_input_device_update_grab(device,
					&resize->grab, surface, time) < 0)
		return;
}

static void
shell_create_drag(struct wl_client *client,
		  struct wl_shell *shell, uint32_t id)
{
}

const struct wl_shell_interface clayland_shell_interface = {
	shell_move,
	shell_resize,
	shell_create_drag
};

