#include "context.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>

typedef struct
{
  guint preedit_start_count;
  guint preedit_changed_count;
  guint preedit_end_count;
  guint commit_count;
  GString *commits;
} SignalProbe;

static gboolean gtk_available = FALSE;

static void
probe_commit_cb (GtkIMContext *context,
                 const char   *str,
                 void* const user_data)
{
  SignalProbe *probe = user_data;

  probe->commit_count++;
  g_string_append (probe->commits, str);
}

static void
probe_preedit_start_cb (GtkIMContext *context,
                        void* const user_data)
{
  SignalProbe *probe = user_data;

  probe->preedit_start_count++;
}

static void
probe_preedit_changed_cb (GtkIMContext *context,
                          void* const user_data)
{
  SignalProbe *probe = user_data;

  probe->preedit_changed_count++;
}

static void
probe_preedit_end_cb (GtkIMContext *context,
                      void* const user_data)
{
  SignalProbe *probe = user_data;

  probe->preedit_end_count++;
}

static void
probe_init (SignalProbe *probe)
{
  memset (probe, 0, sizeof (*probe));
  probe->commits = g_string_new (NULL);
}

static void
probe_clear (SignalProbe *probe)
{
  g_string_free (probe->commits, TRUE);
}

static void
attach_probe (GtkIMContext *context,
              SignalProbe  *probe)
{
  g_signal_connect (context, "commit", G_CALLBACK (probe_commit_cb), probe);
  g_signal_connect (context, "preedit-start", G_CALLBACK (probe_preedit_start_cb), probe);
  g_signal_connect (context, "preedit-changed", G_CALLBACK (probe_preedit_changed_cb), probe);
  g_signal_connect (context, "preedit-end", G_CALLBACK (probe_preedit_end_cb), probe);
}

static void
test_preedit_string_is_always_empty (void)
{
  char *preedit = NULL;
  PangoAttrList *attrs = NULL;
  int cursor_pos = -1;

  if (!gtk_available)
    {
      g_test_skip ("GTK could not initialize a display");
      return;
    }

  GtkIMContext *context = im_context_new ();
  gtk_im_context_get_preedit_string (context, &preedit, &attrs, &cursor_pos);

  g_assert_nonnull (preedit);
  g_assert_cmpstr (preedit, ==, "");
  g_assert_nonnull (attrs);
  g_assert_cmpint (cursor_pos, ==, 0);

  g_free (preedit);
  pango_attr_list_unref (attrs);
  g_object_unref (context);
}

static void
test_no_client_preedit_signals_for_lifecycle_calls (void)
{
  SignalProbe probe;

  if (!gtk_available)
    {
      g_test_skip ("GTK could not initialize a display");
      return;
    }

  GtkIMContext *context = im_context_new ();
  probe_init (&probe);
  attach_probe (context, &probe);

  gtk_im_context_focus_in (context);
  gtk_im_context_reset (context);
  gtk_im_context_set_use_preedit (context, TRUE);
  gtk_im_context_set_use_preedit (context, FALSE);
  gtk_im_context_focus_out (context);

  g_assert_cmpuint (probe.preedit_start_count, ==, 0);
  g_assert_cmpuint (probe.preedit_changed_count, ==, 0);
  g_assert_cmpuint (probe.preedit_end_count, ==, 0);

  probe_clear (&probe);
  g_object_unref (context);
}

static gboolean
retrieve_surrounding_cb (GtkIMContext *context,
                         void* const user_data)
{
  gtk_im_context_set_surrounding_with_selection (context, "abc", -1, 1, 1);

  return TRUE;
}

static void
test_surrounding_signal_round_trip (void)
{
  char *text = NULL;
  int cursor_index = -1;
  int anchor_index = -1;

  if (!gtk_available)
    {
      g_test_skip ("GTK could not initialize a display");
      return;
    }

  GtkIMContext *context = im_context_new ();
  g_signal_connect (context,
                    "retrieve-surrounding",
                    G_CALLBACK (retrieve_surrounding_cb),
                    NULL);

  gboolean ok = gtk_im_context_get_surrounding_with_selection (context,
                                                               &text,
                                                               &cursor_index,
                                                               &anchor_index);

  g_assert_true (ok);
  g_assert_cmpstr (text, ==, "abc");
  g_assert_cmpint (cursor_index, ==, 1);
  g_assert_cmpint (anchor_index, ==, 1);

  g_free (text);
  g_object_unref (context);
}

static void
test_input_properties_round_trip (void)
{
  GtkInputPurpose purpose = GTK_INPUT_PURPOSE_FREE_FORM;
  GtkInputHints hints = GTK_INPUT_HINT_NONE;

  if (!gtk_available)
    {
      g_test_skip ("GTK could not initialize a display");
      return;
    }

  GtkIMContext *context = im_context_new ();
  g_object_set (context,
                "input-purpose", GTK_INPUT_PURPOSE_EMAIL,
                "input-hints", GTK_INPUT_HINT_SPELLCHECK | GTK_INPUT_HINT_LOWERCASE,
                NULL);

  g_object_get (context,
                "input-purpose", &purpose,
                "input-hints", &hints,
                NULL);

  g_assert_cmpint (purpose, ==, GTK_INPUT_PURPOSE_EMAIL);
  g_assert_cmpuint (hints, ==, GTK_INPUT_HINT_SPELLCHECK | GTK_INPUT_HINT_LOWERCASE);

  g_object_unref (context);
}

typedef struct
{
  GdkSurface *surface;
  GdkDevice *device;
} KeyTestEnv;

enum
{
  EVDEV_OFFSET = 8,
  KEY_ESC = 1 + EVDEV_OFFSET,
  KEY_1 = 2 + EVDEV_OFFSET,
  KEY_6 = 7 + EVDEV_OFFSET,
  KEY_E = 18 + EVDEV_OFFSET,
  KEY_T = 20 + EVDEV_OFFSET,
  KEY_U = 22 + EVDEV_OFFSET,
  KEY_O = 24 + EVDEV_OFFSET,
  KEY_A = 30 + EVDEV_OFFSET,
  KEY_C = 46 + EVDEV_OFFSET,
  KEY_N = 49 + EVDEV_OFFSET,
  KEY_APOSTROPHE = 40 + EVDEV_OFFSET,
  KEY_GRAVE = 41 + EVDEV_OFFSET,
  KEY_BACKSPACE = 14 + EVDEV_OFFSET,
  KEY_SPACE = 57 + EVDEV_OFFSET,
};

static gboolean
filter_press (GtkIMContext   *context,
              const KeyTestEnv *env,
              guint           keycode,
              GdkModifierType state)
{
  return gtk_im_context_filter_key (context,
                                    TRUE,
                                    env->surface,
                                    env->device,
                                    GDK_CURRENT_TIME,
                                    keycode,
                                    state,
                                    0);
}

static void
assert_key_sequence_commit (const char      *label,
                            const KeyTestEnv *env,
                            guint            first_keycode,
                            GdkModifierType  first_state,
                            guint            second_keycode,
                            GdkModifierType  second_state,
                            const char      *expected)
{
  GtkIMContext *context = im_context_new ();
  SignalProbe probe;

  probe_init (&probe);
  attach_probe (context, &probe);
  gtk_im_context_focus_in (context);

  g_assert_true (filter_press (context, env, first_keycode, first_state));
  g_assert_true (filter_press (context, env, second_keycode, second_state));
  g_assert_cmpstr (probe.commits->str, ==, expected);
  g_assert_cmpuint (probe.preedit_start_count, ==, 0);
  g_assert_cmpuint (probe.preedit_changed_count, ==, 0);
  g_assert_cmpuint (probe.preedit_end_count, ==, 0);

  if (g_test_verbose ())
    g_test_message ("%s -> %s", label, expected);

  probe_clear (&probe);
  g_object_unref (context);
}

static void
test_key_sequences_on_current_display (void)
{
  KeyTestEnv env;
  SignalProbe probe;

  if (g_getenv ("SILENT_COMPOSE_RUN_KEY_TESTS") == NULL)
    {
      g_test_skip ("set SILENT_COMPOSE_RUN_KEY_TESTS=1 to run display/layout-dependent key tests");
      return;
    }

  if (!gtk_available)
    {
      g_test_skip ("GTK could not initialize a display");
      return;
    }

  GtkWidget *window = gtk_window_new ();
  gtk_window_present (GTK_WINDOW (window));
  while (g_main_context_iteration (NULL, FALSE))
    {
    }

  GdkDisplay *display = gdk_display_get_default ();
  GdkSeat *seat = gdk_display_get_default_seat (display);
  env.surface = gtk_native_get_surface (GTK_NATIVE (window));
  env.device = gdk_seat_get_keyboard (seat);

  if (env.surface == NULL || env.device == NULL)
    {
      gtk_window_destroy (GTK_WINDOW (window));
      g_test_skip ("no realized GTK surface or keyboard device available");
      return;
    }

  assert_key_sequence_commit ("' e", &env, KEY_APOSTROPHE, 0, KEY_E, 0, "é");
  assert_key_sequence_commit ("' c", &env, KEY_APOSTROPHE, 0, KEY_C, 0, "ç");
  assert_key_sequence_commit ("\" u", &env, KEY_APOSTROPHE, GDK_SHIFT_MASK, KEY_U, 0, "ü");
  assert_key_sequence_commit ("` a", &env, KEY_GRAVE, 0, KEY_A, 0, "à");
  assert_key_sequence_commit ("~ n", &env, KEY_GRAVE, GDK_SHIFT_MASK, KEY_N, 0, "ñ");
  assert_key_sequence_commit ("^ o", &env, KEY_6, GDK_SHIFT_MASK, KEY_O, 0, "ô");
  assert_key_sequence_commit ("' Space", &env, KEY_APOSTROPHE, 0, KEY_SPACE, 0, "'");
  assert_key_sequence_commit ("' t", &env, KEY_APOSTROPHE, 0, KEY_T, 0, "'t");
  assert_key_sequence_commit ("' 1", &env, KEY_APOSTROPHE, 0, KEY_1, 0, "'1");

  GtkIMContext *context = im_context_new ();
  probe_init (&probe);
  attach_probe (context, &probe);
  gtk_im_context_focus_in (context);

  g_assert_false (filter_press (context, &env, KEY_C, GDK_CONTROL_MASK));
  g_assert_false (filter_press (context, &env, KEY_U, GDK_CONTROL_MASK));
  g_assert_false (filter_press (context, &env, KEY_1, GDK_ALT_MASK));
  g_assert_false (filter_press (context, &env, KEY_T, GDK_CONTROL_MASK | GDK_SHIFT_MASK));

  g_assert_true (filter_press (context, &env, KEY_APOSTROPHE, 0));
  g_assert_true (filter_press (context, &env, KEY_ESC, 0));
  g_assert_cmpstr (probe.commits->str, ==, "");

  g_assert_true (filter_press (context, &env, KEY_APOSTROPHE, 0));
  g_assert_true (filter_press (context, &env, KEY_BACKSPACE, 0));
  g_assert_cmpuint (probe.preedit_start_count, ==, 0);
  g_assert_cmpuint (probe.preedit_changed_count, ==, 0);
  g_assert_cmpuint (probe.preedit_end_count, ==, 0);

  probe_clear (&probe);
  g_object_unref (context);
  gtk_window_destroy (GTK_WINDOW (window));
}

static gboolean
focus_text_view_cb (void* const user_data)
{
  GtkWidget *text_view = GTK_WIDGET (user_data);

  gtk_widget_grab_focus (text_view);

  return G_SOURCE_REMOVE;
}

static void
activate_manual_app (GtkApplication *app,
                     void* const user_data)
{
  GtkWidget *window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "SilentCompose manual validation");
  gtk_window_set_default_size (GTK_WINDOW (window), 720, 420);

  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_window_set_child (GTK_WINDOW (window), box);

  GtkWidget *label = gtk_label_new ("Type Compose/dead-key sequences here. Expected: completed characters only; no visible pending dead key or preedit underline/highlight.");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_box_append (GTK_BOX (box), label);

  GtkWidget *scrolled = gtk_scrolled_window_new ();
  gtk_widget_set_vexpand (scrolled, TRUE);
  gtk_box_append (GTK_BOX (box), scrolled);

  GtkWidget *text_view = gtk_text_view_new ();
  gtk_text_view_set_monospace (GTK_TEXT_VIEW (text_view), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), text_view);

  gtk_window_present (GTK_WINDOW (window));
  g_idle_add (focus_text_view_cb, text_view);
}

static int
run_manual_app (int argc, char **argv)
{
  GtkApplication *app = gtk_application_new ("dev.silentcompose.ManualTest", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate_manual_app), NULL);
  int status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}

int
main (int argc, char **argv)
{
  for (int i = 1; i < argc; i++)
    {
      if (g_strcmp0 (argv[i], "--manual") == 0)
        return run_manual_app (argc, argv);
    }

  g_test_init (&argc, &argv, NULL);
  gtk_available = gtk_init_check ();

  g_test_add_func ("/silent-compose/preedit-string-empty",
                   test_preedit_string_is_always_empty);
  g_test_add_func ("/silent-compose/no-client-preedit-signals-lifecycle",
                   test_no_client_preedit_signals_for_lifecycle_calls);
  g_test_add_func ("/silent-compose/surrounding-signal-round-trip",
                   test_surrounding_signal_round_trip);
  g_test_add_func ("/silent-compose/input-properties-round-trip",
                   test_input_properties_round_trip);
  g_test_add_func ("/silent-compose/key-sequences-current-display",
                   test_key_sequences_on_current_display);

  return g_test_run ();
}
