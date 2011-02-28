
#include <clutter/clutter.h>
#include <stdlib.h>
#include <wayland-server.h>
#include "clayland.h"

static char *option_socket_name = NULL;

static GOptionEntry option_entries[] = {
	{ "socket", '\0', (GOptionFlags)0, G_OPTION_ARG_STRING,	
	  &option_socket_name, "Wayland socket name", "SOCKET" },
	{ NULL }
};

static void free_options(void)
{
	g_free (option_socket_name);
}

static void
on_term_signal(int signal_number, void *data)
{
	clutter_main_quit();
}

int
main (int argc, char *argv[])
{
	ClutterActor *stage, *hand;
	ClutterColor  stage_color = { 0x61, 0x64, 0x8c, 0xff };
	ClaylandCompositor *compositor;
	GSource      *source;
	GError       *error;
	struct wl_display *display;
	struct wl_event_loop *loop;
	ClutterInitError init;
	guint source_id;

	g_thread_init(NULL);
	clutter_threads_init();

	error = NULL;
	g_atexit(free_options);

	init = clutter_init_with_args (&argc, &argv,
	                               "(a Clutter-based Wayland compositor)",
	                               option_entries, NULL, &error);
	if (init != CLUTTER_INIT_SUCCESS) {
		g_warning ("Unable to initialise Clutter:\n\t%s",
			   error->message);
		g_error_free (error);

		return EXIT_FAILURE;
	}

	display = wl_display_create();
	if (display == NULL) {
		g_warning("failed to create display: %m");
		return EXIT_FAILURE;
	}

	stage = clutter_stage_new ();
	if (!stage)
		return EXIT_FAILURE;

	clutter_actor_set_size (stage, 800, 600);
	clutter_actor_set_name (stage, "Clayland");

	clutter_stage_set_title (CLUTTER_STAGE (stage), "Clayland");
	clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
	/* TODO: allow resizing; need to post geometry updates when we do */
	clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), FALSE);

	hand = clutter_texture_new_from_file ("redhand.png", &error);
	if (hand == NULL)
		g_warning ("image load failed: %s", error->message);
	else {
		clutter_actor_set_reactive (hand, TRUE);
		clutter_actor_set_size (hand, 200, 213);
		clutter_actor_set_position (hand, 200, 200);
		clutter_actor_move_anchor_point_from_gravity (hand,
						      CLUTTER_GRAVITY_CENTER);
		/* Add to our group group */
		clutter_container_add_actor (CLUTTER_CONTAINER (stage), hand);
	}
	/* Show everying */
	clutter_actor_show (stage);

	compositor = clayland_compositor_create(display);
	if (!compositor)
		return EXIT_FAILURE;

	clayland_compositor_add_output(compositor, CLUTTER_CONTAINER(stage));

	if (wl_display_add_socket(display, option_socket_name)) {
		g_warning("failed to add socket: %m");
		g_object_unref(compositor);
		return EXIT_FAILURE;
	}

	loop = wl_display_get_event_loop(display);

	source = clayland_source_new (loop);
	source_id = g_source_attach (source, NULL);
	if (source == NULL || source_id == 0) {
		g_source_unref (source);
		g_object_unref (compositor);
		return EXIT_FAILURE;
	}

	wl_event_loop_add_signal(loop,
				 SIGTERM, on_term_signal, compositor);
	wl_event_loop_add_signal(loop,
				 SIGINT, on_term_signal, compositor);

	clutter_threads_enter();
	clutter_main ();
	clutter_threads_leave();

	g_source_destroy (source);
	g_source_unref (source);
	g_object_unref (compositor);

	return EXIT_SUCCESS;
}

