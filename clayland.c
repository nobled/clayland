#include <stdlib.h>
#include <wayland-server.h>
#include <clutter/clutter.h>

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>

#include "clayland.h"

typedef struct Clayland {
	ClutterActor      *hand;
	ClutterActor      *stage;
	GSource           *source;
	struct wl_display *display;
	struct wl_event_loop *loop;

	gint stage_width;
	gint stage_height;
} Clayland;

static const char socket_name[] = "\0wayland";

static gboolean
on_button_press_event (ClutterActor *actor,
		       ClutterEvent *event,
		       Clayland     *clayland)
{
	gfloat x, y;

	clutter_event_get_coords (event, &x, &y);

	g_print ("*** button press event (button:%d) at %.2f, %.2f on %s ***\n",
		 clutter_event_get_button (event),
		 x, y,
		 clutter_actor_get_name (actor));

	clutter_actor_hide (actor);

	return TRUE;
}

static gboolean
input_cb (ClutterActor *stage, ClutterEvent *event, gpointer      data)
{
	Clayland *clayland = data;

	if (event->type == CLUTTER_KEY_RELEASE) {
		g_print ("*** key press event (key:%c) ***\n",
			 clutter_event_get_key_symbol (event));

		if (clutter_event_get_key_symbol (event) == CLUTTER_KEY_q) {
			clutter_main_quit ();

			return TRUE;
		}
	}

	clutter_actor_show (clayland->hand);

	return FALSE;
}

static void
on_term_signal(int signal_number, void *data)
{
	Clayland *clayland = data;

	clutter_main_quit();
}

int
main (int argc, char *argv[])
{
	ClutterAlpha *alpha;
	ClutterActor *stage;
	ClutterColor  stage_color = { 0x61, 0x64, 0x8c, 0xff };
	Clayland     *clayland;
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

	clayland = g_new (Clayland, 1);
	clayland->stage = stage;

	clayland->display = wl_display_create();
	if (clayland->display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return 1;
	}

	clayland->loop = wl_display_get_event_loop(clayland->display);

	clayland->source = wl_glib_source_new(clayland->loop);
	g_source_attach(clayland->source, NULL);

	if (wl_display_add_socket(clayland->display, NULL)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}

	loop = wl_display_get_event_loop(clayland->display);
	wl_event_loop_add_signal(loop, SIGTERM, on_term_signal, clayland);
	wl_event_loop_add_signal(loop, SIGINT, on_term_signal, clayland);

	clayland->hand = clutter_texture_new_from_file ("redhand.png", &error);
	if (clayland->hand == NULL)
		g_error ("image load failed: %s", error->message);

	g_free (file);

	clayland->stage_width = clutter_actor_get_width (stage);
	clayland->stage_height = clutter_actor_get_height (stage);

	clutter_actor_set_reactive (clayland->hand, TRUE);
	clutter_actor_set_size (clayland->hand, 200, 213);
	clutter_actor_set_position (clayland->hand, 200, 200);
	clutter_actor_move_anchor_point_from_gravity (clayland->hand,
						      CLUTTER_GRAVITY_CENTER);

	/* Add to our group group */
	clutter_container_add_actor (CLUTTER_CONTAINER (stage), clayland->hand);
	g_signal_connect (clayland->hand, "button-press-event",
			  G_CALLBACK (on_button_press_event),
			  clayland);

	/* Show everying */
	clutter_actor_show (stage);

	g_signal_connect (stage, "key-release-event",
			  G_CALLBACK (input_cb),
			  clayland);

	clutter_main ();

	wl_display_destroy (clayland->display);
	g_free (clayland);

	return EXIT_SUCCESS;
}
