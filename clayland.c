#include <stdlib.h>
#include <wayland-server.h>
#include <clutter/clutter.h>

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>

#include "clayland.h"

typedef struct ClaylandCompositor {
	ClutterActor      *hand;
	ClutterActor      *stage;
	GSource           *source;
	struct wl_display *display;
	struct wl_event_loop *loop;

	struct wl_compositor compositor;

	gint stage_width;
	gint stage_height;
} ClaylandCompositor;

typedef struct ClaylandSurface {
	struct wl_surface	 surface;
	ClaylandCompositor	*compositor;
	ClutterActor		*hand;
} ClaylandSurface;

static gboolean
event_cb (ClutterActor *stage, ClutterEvent *event, gpointer      data)
{
	ClaylandCompositor *compositor = data;

	switch (event->type) {
	case CLUTTER_NOTHING:
	case CLUTTER_KEY_PRESS:
	case CLUTTER_KEY_RELEASE:
	case CLUTTER_STAGE_STATE:
	case CLUTTER_DESTROY_NOTIFY:
	case CLUTTER_CLIENT_MESSAGE:
	case CLUTTER_DELETE:
	case CLUTTER_SCROLL:
		break;

	case CLUTTER_MOTION:
		fprintf(stderr, "motion %p, %f,%f\n",
			event->motion.source,
			event->motion.x, event->motion.y);
		break;

	case CLUTTER_ENTER:
		fprintf(stderr, "enter %p\n", event->any.source);
		break;

	case CLUTTER_LEAVE:
		fprintf(stderr, "leave %p\n", event->any.source);
		break;

	case CLUTTER_BUTTON_PRESS:
		fprintf(stderr, "button press %p\n", event->any.source);
		break;

	case CLUTTER_BUTTON_RELEASE:
		fprintf(stderr, "button release %p\n", event->any.source);
		break;
	}

	return TRUE;
}

static void
on_term_signal(int signal_number, void *data)
{
	clutter_main_quit();
}

static void
surface_destroy(struct wl_client *client,
		struct wl_surface *surface)
{
	wl_resource_destroy(&surface->resource, client);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, struct wl_buffer *buffer)
{
	ClaylandSurface *csurface = (ClaylandSurface *) surface;
}

static void
surface_map(struct wl_client *client,
	    struct wl_surface *surface,
	    int32_t x, int32_t y, int32_t width, int32_t height)
{
	ClaylandSurface *csurface = (ClaylandSurface *) surface;

	clutter_actor_set_size (csurface->hand, x, y);
	clutter_actor_set_position (csurface->hand, width, height);
	clutter_actor_show (csurface->hand);
}

static void
surface_damage(struct wl_client *client,
	       struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	ClaylandSurface *csurface = (ClaylandSurface *) surface;
}

const static struct wl_surface_interface surface_interface = {
	surface_destroy,
	surface_attach,
	surface_map,
	surface_damage
};

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
	ClaylandSurface *surface = (ClaylandSurface *) resource;
	ClaylandCompositor *compositor = surface->compositor;
	struct wl_listener *l, *next;
	uint32_t time;

	time = get_time();
	wl_list_for_each_safe(l, next,
			      &surface->surface.destroy_listener_list, link)
		l->func(l, &surface->surface, time);

	g_object_unref (surface->hand);

	g_free(surface);
}

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_compositor *compositor, uint32_t id)
{
	ClaylandCompositor *clayland = (ClaylandCompositor *) compositor;
	ClaylandSurface *surface;
	GError       *error;

	surface = g_new (ClaylandSurface, 1);

	error = NULL;
	surface->hand = clutter_texture_new_from_file ("redhand.png", &error);
	if (surface->hand == NULL) {
		g_error ("image load failed: %s", error->message);
		return;
	}

	surface->surface.resource.destroy = destroy_surface;

	surface->surface.resource.object.id = id;
	surface->surface.resource.object.interface = &wl_surface_interface;
	surface->surface.resource.object.implementation =
		(void (**)(void)) &surface_interface;
	surface->surface.client = client;

	wl_client_add_resource(client, &surface->surface.resource);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
};

ClaylandCompositor *
clayland_compositor_create(ClutterActor *stage)
{
	ClaylandCompositor *compositor;

	compositor = g_new (ClaylandCompositor, 1);
	compositor->stage = stage;

	compositor->display = wl_display_create();
	if (compositor->display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		g_free(compositor);
		return NULL;
	}

	compositor->loop = wl_display_get_event_loop(compositor->display);
	compositor->source = wl_glib_source_new(compositor->loop);
	g_source_attach(compositor->source, NULL);

	if (wl_display_add_socket(compositor->display, NULL)) {
		fprintf(stderr, "failed to add socket: %m\n");
		g_free(compositor);
		return NULL;
	}

	if (wl_compositor_init(&compositor->compositor,
			       &compositor_interface,
			       compositor->display) < 0) {
		g_free(compositor);
		return NULL;
	}

	wl_event_loop_add_signal(compositor->loop,
				 SIGTERM, on_term_signal, compositor);
	wl_event_loop_add_signal(compositor->loop,
				 SIGINT, on_term_signal, compositor);

	return compositor;
}

int
main (int argc, char *argv[])
{
	ClutterAlpha *alpha;
	ClutterActor *stage;
	ClutterColor  stage_color = { 0x61, 0x64, 0x8c, 0xff };
	ClaylandCompositor *compositor;
	struct wl_event_loop *loop;
	gint          i;
	GError       *error;
	gchar        *file;

	error = NULL;

	clutter_init_with_args (&argc, &argv, NULL, NULL, NULL, &error);
	if (error) {
		g_warning ("Unable to initialise Clutter:\n%s",
			   error->message);
		g_error_free (error);

		return EXIT_FAILURE;
	}

	stage = clutter_stage_get_default ();
	clutter_actor_set_size (stage, 800, 600);
	clutter_actor_set_name (stage, "Clayland");

	clutter_stage_set_title (CLUTTER_STAGE (stage), "Clayland");
	clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
	clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

	compositor = clayland_compositor_create(stage);

	compositor->hand = clutter_texture_new_from_file ("redhand.png", &error);
	if (compositor->hand == NULL)
		g_error ("image load failed: %s", error->message);

	g_free (file);

	compositor->stage_width = clutter_actor_get_width (stage);
	compositor->stage_height = clutter_actor_get_height (stage);

	clutter_actor_set_reactive (compositor->hand, TRUE);
	clutter_actor_set_size (compositor->hand, 200, 213);
	clutter_actor_set_position (compositor->hand, 200, 200);
	clutter_actor_move_anchor_point_from_gravity (compositor->hand,
						      CLUTTER_GRAVITY_CENTER);

	/* Add to our group group */
	clutter_container_add_actor (CLUTTER_CONTAINER (stage), compositor->hand);
	/* Show everying */
	clutter_actor_show (stage);

	g_signal_connect (stage, "captured-event",
			  G_CALLBACK (event_cb),
			  compositor);

	clutter_main ();

	wl_display_destroy (compositor->display);
	g_free (compositor);

	return EXIT_SUCCESS;
}
