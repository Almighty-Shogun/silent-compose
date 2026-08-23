#include "context.h"

#include <gtk/gtk.h>
#include <string.h>

/**
 * Counters and captured text for one input context under test.
 *
 * The preedit counters are the actual assertion of this suite: any preedit
 * signal reaching the client is the failure the module exists to prevent.
 */
typedef struct
{
    guint preedit_start_count;
    guint preedit_changed_count;
    guint preedit_end_count;
    guint commit_count;
    GString* commits;
} SignalProbe;

/**
 * Whether GTK managed to initialize a display for this run.
 *
 * Headless CI has no display, so every display-dependent test skips instead of
 * failing when this stays FALSE.
 */
static gboolean gtk_available = FALSE;

/**
 * Record committed text emitted by the context under test.
 *
 * Commits are appended rather than replaced, so a sequence that wrongly emits
 * two fragments is visible in the comparison.
 */
static void probe_commit_cb(GtkIMContext* _, const char* str, void* const data)
{
    SignalProbe* probe = data;

    probe->commit_count++;

    g_string_append(probe->commits, str);
}

/**
 * Count client-visible preedit-start signals.
 *
 * Any non-zero count means the wrapper leaked a delegated signal to the
 * application.
 */
static void probe_preedit_start_cb(GtkIMContext* _, void* const data)
{
    SignalProbe* probe = data;

    probe->preedit_start_count++;
}

/**
 * Count client-visible preedit-changed signals.
 *
 * This is the signal that would draw a pending dead key, so it must never
 * reach the client.
 */
static void probe_preedit_changed_cb(GtkIMContext* _, void* const data)
{
    SignalProbe* probe = data;

    probe->preedit_changed_count++;
}

/**
 * Count client-visible preedit-end signals.
 *
 * It is counted alongside the other two so an unbalanced pair cannot pass
 * unnoticed.
 */
static void probe_preedit_end_cb(GtkIMContext* _, void* const data)
{
    SignalProbe* probe = data;

    probe->preedit_end_count++;
}

/**
 * Initialize a signal probe before attaching it to an input context.
 *
 * The whole struct is zeroed so a probe reused across contexts never carries
 * counts from an earlier one.
 */
static void probe_init(SignalProbe* probe)
{
    memset(probe, 0, sizeof (*probe));

    probe->commits = g_string_new(NULL);
}

/**
 * Release dynamic storage owned by a signal probe.
 *
 * Only the commit string is allocated; the counters live in the caller's own
 * stack frame.
 */
static void probe_clear(const SignalProbe* probe)
{
    g_string_free(probe->commits, TRUE);
}

/**
 * Attach commit and preedit counters to the input context under test.
 *
 * The probe listens on the wrapper rather than the delegate, which is exactly
 * what an application would see.
 */
static void attach_probe(GtkIMContext* context, SignalProbe* probe)
{
    g_signal_connect(context, "commit", G_CALLBACK (probe_commit_cb), probe);
    g_signal_connect(context, "preedit-start", G_CALLBACK (probe_preedit_start_cb), probe);
    g_signal_connect(context, "preedit-changed", G_CALLBACK (probe_preedit_changed_cb), probe);
    g_signal_connect(context, "preedit-end", G_CALLBACK (probe_preedit_end_cb), probe);
}

/**
 * Verify the wrapper never exposes visible preedit text.
 *
 * A non-NULL attribute list with an empty string is what satisfies the GTK API
 * without giving the client anything to draw.
 */
static void preedit_string_is_always_empty(void)
{
    int cursor = -1;

    char* preedit = NULL;
    PangoAttrList* attrs = NULL;

    if (!gtk_available)
    {
        g_test_skip("GTK could not initialize a display");

        return;
    }

    GtkIMContext* context = im_context_new();

    gtk_im_context_get_preedit_string(context, &preedit, &attrs, &cursor);

    g_assert_nonnull(preedit);
    g_assert_cmpstr(preedit, ==, "");
    g_assert_nonnull(attrs);
    g_assert_cmpint(cursor, ==, 0);

    g_free(preedit);

    pango_attr_list_unref(attrs);
    g_object_unref(context);
}

/**
 * Verify lifecycle calls do not emit client-visible preedit signals.
 *
 * Focus, reset, and use-preedit changes all reach the delegate, so they are
 * the most likely way a suppressed signal would escape.
 */
static void no_client_preedit_signals_for_lifecycle_calls(void)
{
    SignalProbe probe;

    if (!gtk_available)
    {
        g_test_skip("GTK could not initialize a display");

        return;
    }

    GtkIMContext* context = im_context_new();

    probe_init(&probe);
    attach_probe(context, &probe);

    gtk_im_context_focus_in(context);
    gtk_im_context_reset(context);
    gtk_im_context_set_use_preedit(context, TRUE);
    gtk_im_context_set_use_preedit(context, FALSE);
    gtk_im_context_focus_out(context);

    g_assert_cmpuint(probe.preedit_start_count, ==, 0);
    g_assert_cmpuint(probe.preedit_changed_count, ==, 0);
    g_assert_cmpuint(probe.preedit_end_count, ==, 0);

    probe_clear(&probe);
    g_object_unref(context);
}

/**
 * Provide surrounding text when the context requests it.
 *
 * It stands in for the application, so the request has to be answered on the
 * context that emitted it.
 */
static gboolean retrieve_surrounding_cb(GtkIMContext* context, void* const _)
{
    gtk_im_context_set_surrounding_with_selection(context, "abc", -1, 1, 1);

    return TRUE;
}

/**
 * Verify surrounding-text requests round-trip through the wrapper.
 *
 * Both the delegate's request and the client's answer cross the wrapper, so a
 * missing forward would break input methods that rely on context.
 */
static void surrounding_signal_round_trip(void)
{
    int cursor = -1;
    int anchor = -1;

    char* text = NULL;

    if (!gtk_available)
    {
        g_test_skip("GTK could not initialize a display");

        return;
    }

    GtkIMContext* context = im_context_new();

    g_signal_connect(context, "retrieve-surrounding", G_CALLBACK (retrieve_surrounding_cb), NULL);

    const gboolean ok = gtk_im_context_get_surrounding_with_selection(context, &text, &cursor, &anchor);

    g_assert_true(ok);
    g_assert_cmpstr(text, ==, "abc");
    g_assert_cmpint(cursor, ==, 1);
    g_assert_cmpint(anchor, ==, 1);

    g_free(text);

    g_object_unref(context);
}

/**
 * Verify input-purpose and input-hints properties round-trip through the wrapper.
 *
 * The properties are overridden rather than installed, which is where a wrong
 * property id would surface.
 */
static void input_properties_round_trip(void)
{
    GtkInputHints hints = GTK_INPUT_HINT_NONE;
    GtkInputPurpose purpose = GTK_INPUT_PURPOSE_FREE_FORM;

    if (!gtk_available)
    {
        g_test_skip("GTK could not initialize a display");

        return;
    }

    GtkIMContext* context = im_context_new();

    g_object_set(context, "input-purpose", GTK_INPUT_PURPOSE_EMAIL, "input-hints",
                 GTK_INPUT_HINT_SPELLCHECK | GTK_INPUT_HINT_LOWERCASE, NULL);

    g_object_get(context, "input-purpose", &purpose, "input-hints", &hints, NULL);

    g_assert_cmpint(purpose, ==, GTK_INPUT_PURPOSE_EMAIL);
    g_assert_cmpuint(hints, ==, GTK_INPUT_HINT_SPELLCHECK | GTK_INPUT_HINT_LOWERCASE);

    g_object_unref(context);
}

/**
 * Display objects needed to synthesize key events.
 *
 * GTK's modern filtering API takes a surface and a keyboard device, so both
 * are captured once and reused across sequences.
 */
typedef struct
{
    GdkDevice* device;
    GdkSurface* surface;
} KeyTestEnv;

/**
 * Hardware keycodes used to synthesize key presses.
 *
 * GTK expects XKB keycodes, which sit 8 above the evdev numbers, so each entry
 * carries the offset explicitly rather than hiding it in a constant.
 */
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

/**
 * Send a synthetic key press through GTK's modern key filtering API.
 *
 * Going through gtk_im_context_filter_key() exercises the same entry point a
 * real widget uses, keycode translation included.
 */
static gboolean filter_press(GtkIMContext* context, const KeyTestEnv* env, const guint code, const GdkModifierType state)
{
    return gtk_im_context_filter_key(context, TRUE, env->surface, env->device, GDK_CURRENT_TIME, code, state, 0);
}

/**
 * Assert a two-key GTK input sequence emits only the expected commit text.
 *
 * Both key presses must be filtered, and all three preedit counters must stay
 * at zero, so the sequence is checked end to end.
 */
static void assert_key_sequence_commit(
    const char* label,
    const KeyTestEnv* env,
    const guint first_key,
    const GdkModifierType first_mods,
    const guint second_key,
    const GdkModifierType second_mods,
    const char* expected)
{
    GtkIMContext* context = im_context_new();

    SignalProbe probe;

    probe_init(&probe);
    attach_probe(context, &probe);
    gtk_im_context_focus_in(context);

    g_assert_true(filter_press (context, env, first_key, first_mods));
    g_assert_true(filter_press (context, env, second_key, second_mods));

    g_assert_cmpstr(probe.commits->str, ==, expected);
    g_assert_cmpuint(probe.preedit_start_count, ==, 0);
    g_assert_cmpuint(probe.preedit_changed_count, ==, 0);
    g_assert_cmpuint(probe.preedit_end_count, ==, 0);

    if (g_test_verbose())
        g_message("%s -> %s", label, expected);

    probe_clear(&probe);
    g_object_unref(context);
}

/**
 * Run display/layout-dependent end-to-end key filtering checks when enabled.
 *
 * The results depend on the session's keyboard layout, so the test is opt-in
 * through SILENT_COMPOSE_RUN_KEY_TESTS and skips without a realized surface.
 */
static void key_sequences_on_current_display(void)
{
    KeyTestEnv env;
    SignalProbe probe;

    if (g_getenv("SILENT_COMPOSE_RUN_KEY_TESTS") == NULL)
    {
        g_test_skip("set SILENT_COMPOSE_RUN_KEY_TESTS=1 to run display/layout-dependent key tests");

        return;
    }

    if (!gtk_available)
    {
        g_test_skip("GTK could not initialize a display");

        return;
    }

    GtkWidget* window = gtk_window_new();

    gtk_window_present(GTK_WINDOW(window));

    while (g_main_context_iteration(NULL, FALSE)) { }

    GdkDisplay* display = gdk_display_get_default();
    GdkSeat* seat = gdk_display_get_default_seat(display);

    env.surface = gtk_native_get_surface(GTK_NATIVE(window));
    env.device = gdk_seat_get_keyboard(seat);

    if (env.surface == NULL || env.device == NULL)
    {
        gtk_window_destroy(GTK_WINDOW(window));
        g_test_skip("no realized GTK surface or keyboard device available");

        return;
    }

    assert_key_sequence_commit("' e", &env, KEY_APOSTROPHE, 0, KEY_E, 0, "é");
    assert_key_sequence_commit("' c", &env, KEY_APOSTROPHE, 0, KEY_C, 0, "ç");
    assert_key_sequence_commit("\" u", &env, KEY_APOSTROPHE, GDK_SHIFT_MASK, KEY_U, 0, "ü");
    assert_key_sequence_commit("` a", &env, KEY_GRAVE, 0, KEY_A, 0, "à");
    assert_key_sequence_commit("~ n", &env, KEY_GRAVE, GDK_SHIFT_MASK, KEY_N, 0, "ñ");
    assert_key_sequence_commit("^ o", &env, KEY_6, GDK_SHIFT_MASK, KEY_O, 0, "ô");
    assert_key_sequence_commit("' Space", &env, KEY_APOSTROPHE, 0, KEY_SPACE, 0, "'");
    assert_key_sequence_commit("' t", &env, KEY_APOSTROPHE, 0, KEY_T, 0, "'t");
    assert_key_sequence_commit("' 1", &env, KEY_APOSTROPHE, 0, KEY_1, 0, "'1");

    GtkIMContext* context = im_context_new();

    probe_init(&probe);
    attach_probe(context, &probe);
    gtk_im_context_focus_in(context);

    g_assert_false(filter_press (context, &env, KEY_C, GDK_CONTROL_MASK));
    g_assert_false(filter_press (context, &env, KEY_U, GDK_CONTROL_MASK));
    g_assert_false(filter_press (context, &env, KEY_1, GDK_ALT_MASK));
    g_assert_false(filter_press (context, &env, KEY_T, GDK_CONTROL_MASK | GDK_SHIFT_MASK));

    g_assert_true(filter_press (context, &env, KEY_APOSTROPHE, 0));
    g_assert_true(filter_press (context, &env, KEY_ESC, 0));
    g_assert_cmpstr(probe.commits->str, ==, "");

    g_assert_true(filter_press (context, &env, KEY_APOSTROPHE, 0));
    g_assert_true(filter_press (context, &env, KEY_BACKSPACE, 0));
    g_assert_cmpuint(probe.preedit_start_count, ==, 0);
    g_assert_cmpuint(probe.preedit_changed_count, ==, 0);
    g_assert_cmpuint(probe.preedit_end_count, ==, 0);

    probe_clear(&probe);
    g_object_unref(context);
    gtk_window_destroy(GTK_WINDOW(window));
}

/**
 * Move keyboard focus into the manual validation text view after presentation.
 *
 * Focus is grabbed from an idle callback because the widget is not realized
 * yet while the window is being presented.
 */
static gboolean focus_text_view_cb(void* const data)
{
    GtkWidget* text_view = GTK_WIDGET(data);

    gtk_widget_grab_focus(text_view);

    return G_SOURCE_REMOVE;
}

/**
 * Build the manual validation window used by --manual.
 *
 * It exists for the checks no automated test can make: whether a pending dead
 * key is visible on screen while the user types.
 */
static void activate_manual_app(GtkApplication* app, void* const _)
{
    GtkWidget* window = gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window), "SilentCompose manual validation");
    gtk_window_set_default_size(GTK_WINDOW(window), 720, 420);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_window_set_child(GTK_WINDOW(window), box);

    GtkWidget* label = gtk_label_new("Type Compose/dead-key sequences here. Expected: completed characters only; "
        "no visible pending dead key or preedit underline/highlight.");

    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_box_append(GTK_BOX(box), label);

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(box), scrolled);

    GtkWidget* text_view = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text_view);

    gtk_window_present(GTK_WINDOW(window));
    g_idle_add(focus_text_view_cb, text_view);
}

/**
 * Run the optional manual validation application.
 *
 * It replaces the test suite entirely for that run, because a GtkApplication
 * owns the main loop the tests would otherwise drive.
 */
static int run_manual_app(const int argc, char** argv)
{
    GtkApplication* app = gtk_application_new("dev.silentcompose.ManualTest", G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK (activate_manual_app), NULL);

    const int status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);

    return status;
}

/**
 * Register GTK input-method tests or launch the manual validation app.
 *
 * --manual is handled before g_test_init() so the option is never consumed by
 * the test framework.
 */
int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (g_strcmp0(argv[i], "--manual") == 0)
            return run_manual_app(argc, argv);
    }

    g_test_init(&argc, &argv, NULL);

    gtk_available = gtk_init_check();

    g_test_add_func("/silent-compose/preedit-string-empty", preedit_string_is_always_empty);
    g_test_add_func("/silent-compose/no-client-preedit-signals-lifecycle", no_client_preedit_signals_for_lifecycle_calls);
    g_test_add_func("/silent-compose/surrounding-signal-round-trip", surrounding_signal_round_trip);
    g_test_add_func("/silent-compose/input-properties-round-trip", input_properties_round_trip);
    g_test_add_func("/silent-compose/key-sequences-current-display", key_sequences_on_current_display);

    return g_test_run();
}
