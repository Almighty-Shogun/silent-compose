#include "context.h"

/**
 * State for one Silent Compose GTK input context.
 *
 * The wrapper owns a GtkIMContextSimple delegate that keeps GTK's native
 * compose and dead-key behavior. This type only filters what is exposed to
 * applications and mirrors the two overridden properties onto the delegate.
 */
typedef struct
{
    /*
     * Required first field for a GtkIMContext subtype.  GTK/GObject access this
     * through type casts and virtual method dispatch.
     */
    GtkIMContext parent_instance G_GNUC_UNUSED;

    /*
     * GtkIMContextSimple keeps GTK's native compose/dead-key behavior.  This
     * wrapper only filters what is exposed to applications.
     */
    GtkIMContext* delegate;
    GtkInputPurpose input_purpose;
    GtkInputHints input_hints;
    gboolean debug;
} ScIMContext;

/**
 * Class struct for the Silent Compose GTK input context.
 *
 * It carries no class-owned data and exists so the wrapper can override the
 * GtkIMContext virtual methods.
 */
typedef struct
{
    /* Required first field for the class struct. */
    GtkIMContextClass parent_class G_GNUC_UNUSED;
} ScIMContextClass;

#define SC_IM_CONTEXT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SC_TYPE_IM_CONTEXT, ScIMContext))

/**
 * Property identifiers overridden from GtkIMContext.
 *
 * Both properties are overridden rather than newly installed, so applications
 * keep using the names GtkIMContext already defines for them.
 */
enum
{
    PROP_INPUT_PURPOSE = 1,
    PROP_INPUT_HINTS,
};

/**
 * GObject class initializer declared before the type-generation macro.
 *
 * The macro below references the symbol as it expands, so the declaration has
 * to come first.
 */
static void sc_im_context_class_init(ScIMContextClass* klass);
#ifndef SC_STATIC_TYPE
/**
 * Dynamic-type class finalizer declared before the type-generation macro.
 *
 * Only the dynamic build needs it, which is why the declaration sits inside
 * the same guard as its definition.
 */
static void sc_im_context_class_finalize(ScIMContextClass* klass);
#endif
/**
 * GObject instance initializer declared before the type-generation macro.
 *
 * The macro below references the symbol as it expands, so the declaration has
 * to come first.
 */
static void sc_im_context_init(ScIMContext* self);

#ifdef SC_STATIC_TYPE
G_DEFINE_TYPE(ScIMContext, sc_im_context, GTK_TYPE_IM_CONTEXT)
#else
G_DEFINE_DYNAMIC_TYPE(ScIMContext, sc_im_context, GTK_TYPE_IM_CONTEXT)
#endif

/**
 * Return TRUE when verbose GTK IM module logging was requested through the environment.
 *
 * SILENT_COMPOSE_DEBUG is sampled once per context because an application
 * cannot usefully change it after its input method has been loaded.
 */
static gboolean debug_enabled(void)
{
    const char* value = g_getenv("SILENT_COMPOSE_DEBUG");

    return value != NULL && value[0] != '\0' && g_strcmp0(value, "0") != 0;
}

/**
 * Emit debug logs only when explicitly requested by SILENT_COMPOSE_DEBUG.
 *
 * Keeping the check here rather than at the call sites leaves the delegate
 * forwarding functions short and free of conditionals.
 */
static void debug(const ScIMContext* self, const char* message)
{
    if (self->debug)
        g_debug("%s", message);
}

/**
 * GTK adds signals across releases; look up optional ones before connecting.
 *
 * This is what allows the module to build and run against GTK 4 versions that
 * predate signals such as invalid-composition.
 */
static gboolean signal_exists(const GType type, const char* name)
{
    return g_signal_lookup(name, type) != 0;
}

/**
 * Forward only completed text from GtkIMContextSimple to the client context.
 *
 * Commit is the one delegate signal that reaches the application, which is
 * what keeps a composition invisible until its final character exists.
 */
static void delegate_commit_cb(GtkIMContext* _, const char* str, void* const data)
{
    ScIMContext* self = SC_IM_CONTEXT(data);

    if (self->debug)
        g_debug("forwarding commit from delegated GtkIMContextSimple: “%s”", str);

    g_signal_emit_by_name(self, "commit", str);
}

/**
 * Let the delegate ask the client for surrounding text through this wrapper.
 *
 * The delegate is attached to this wrapper rather than to the widget, so its
 * request has to be re-emitted here for the application to answer it.
 */
static gboolean delegate_retrieve_surrounding_cb(GtkIMContext* _, void* const data)
{
    ScIMContext* self = SC_IM_CONTEXT(data);

    gboolean handled = FALSE;

    debug(self, "forwarding retrieve-surrounding request");

    g_signal_emit_by_name(self, "retrieve-surrounding", &handled);

    return handled;
}

/**
 * Forward deletion requests from the delegate to the real client.
 *
 * The handled flag travels back unchanged, so the delegate learns whether the
 * application actually removed the text.
 */
static gboolean delegate_delete_surrounding_cb(GtkIMContext* _, const int offset, const int n, void* const data)
{
    ScIMContext* self = SC_IM_CONTEXT(data);

    gboolean handled = FALSE;

    debug(self, "forwarding delete-surrounding request");

    g_signal_emit_by_name(self, "delete-surrounding", offset, n, &handled);

    return handled;
}

/**
 * Preserve GTK's invalid-composition behavior when the signal exists.
 *
 * The signal is only present on newer GTK versions, so it is looked up before
 * being re-emitted and the event is dropped when the client cannot receive it.
 */
static gboolean delegate_invalid_composition_cb(GtkIMContext* _, const char* str, void* const data)
{
    ScIMContext* self = SC_IM_CONTEXT(data);

    gboolean handled = FALSE;

    if (!signal_exists(G_OBJECT_TYPE(self), "invalid-composition"))
        return FALSE;

    debug(self, "forwarding invalid-composition");
    g_signal_emit_by_name(self, "invalid-composition", str, &handled);

    return handled;
}

/**
 * Swallow every delegated preedit signal.
 *
 * This is the module's critical invariant: applications must never see a
 * pending dead key, underline, highlighted range, popup, or other temporary
 * preedit UI from this module.
 */
static void delegate_preedit_ignored_cb(GtkIMContext* _, void* const data)
{
    const ScIMContext* self = SC_IM_CONTEXT(data);

    debug(self, "suppressed delegated preedit signal");
}

/**
 * Attach the wrapper to its delegate.
 *
 * Commit and surrounding-text signals are forwarded to the client, while the
 * three preedit signals are bound to the handler that discards them.
 */
static void connect_delegate(ScIMContext* self)
{
    g_signal_connect(self->delegate, "commit", G_CALLBACK (delegate_commit_cb), self);
    g_signal_connect(self->delegate, "retrieve-surrounding", G_CALLBACK (delegate_retrieve_surrounding_cb), self);
    g_signal_connect(self->delegate, "delete-surrounding", G_CALLBACK (delegate_delete_surrounding_cb), self);

    if (signal_exists(G_OBJECT_TYPE(self->delegate), "invalid-composition"))
        g_signal_connect(self->delegate, "invalid-composition", G_CALLBACK (delegate_invalid_composition_cb), self);

    g_signal_connect(self->delegate, "preedit-start", G_CALLBACK (delegate_preedit_ignored_cb), self);
    g_signal_connect(self->delegate, "preedit-changed", G_CALLBACK (delegate_preedit_ignored_cb), self);
    g_signal_connect(self->delegate, "preedit-end", G_CALLBACK (delegate_preedit_ignored_cb), self);
}

/**
 * Keep overridden input-purpose/input-hints properties mirrored to the delegate.
 *
 * GtkIMContextSimple adapts its own behavior to purpose and hints, so the
 * values are cached here and pushed down as soon as they change.
 */
static void set_property(GObject* object, const guint prop_id, const GValue* value, GParamSpec* pspec)
{
    ScIMContext* self = SC_IM_CONTEXT(object);

    switch (prop_id)
    {
        case PROP_INPUT_PURPOSE:
            self->input_purpose = g_value_get_enum(value);

            if (self->delegate != NULL)
                g_object_set(self->delegate, "input-purpose", self->input_purpose, NULL);

            break;

        case PROP_INPUT_HINTS:
            self->input_hints = g_value_get_flags(value);

            if (self->delegate != NULL)
                g_object_set(self->delegate, "input-hints", self->input_hints, NULL);

            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

/**
 * Report the wrapper's cached property values.
 *
 * The cached copies answer immediately and stay correct even when a property
 * is read before the delegate has been configured.
 */
static void get_property(GObject* object, const guint prop_id, GValue* value, GParamSpec* pspec)
{
    const ScIMContext* self = SC_IM_CONTEXT(object);

    switch (prop_id)
    {
        case PROP_INPUT_PURPOSE:
            g_value_set_enum(value, self->input_purpose);
            break;

        case PROP_INPUT_HINTS:
            g_value_set_flags(value, self->input_hints);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

/**
 * Disconnect delegate signals before releasing the delegate context.
 *
 * Handlers are removed by data pointer first, so a late signal can never reach
 * a wrapper that is already being torn down.
 */
static void dispose(GObject* object)
{
    ScIMContext* self = SC_IM_CONTEXT(object);

    if (self->delegate != NULL)
    {
        g_signal_handlers_disconnect_by_data(self->delegate, self);
        g_clear_object(&self->delegate);
    }

    G_OBJECT_CLASS(sc_im_context_parent_class)->dispose(object);
}

/**
 * Forward client-widget ownership/state to GtkIMContextSimple.
 *
 * The delegate needs the widget for its own surrounding-text and on-screen
 * keyboard handling.
 */
static void set_client_widget(GtkIMContext* context, GtkWidget* widget)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    gtk_im_context_set_client_widget(self->delegate, widget);
}

/**
 * Always return an empty preedit string.
 *
 * Returning attrs with an empty string keeps the API contract while preventing
 * any visible preedit UI from being drawn.
 */
static void get_preedit_string(GtkIMContext* _, char** str, PangoAttrList** attrs, int* cursor)
{
    if (str != NULL)
        *str = g_strdup("");

    if (attrs != NULL)
        *attrs = pango_attr_list_new();

    if (cursor != NULL)
        *cursor = 0;
}

/**
 * Delegate key filtering to GtkIMContextSimple.
 *
 * Native GTK compose behavior is kept intact; the visible preedit output it
 * would normally produce is suppressed by the signal handlers instead.
 */
static gboolean filter_keypress(GtkIMContext* context, GdkEvent* event)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    const gboolean handled = gtk_im_context_filter_keypress(self->delegate, event);

    if (self->debug)
        g_debug("delegated key event handled=%s", handled ? "true" : "false");

    return handled;
}

/**
 * Mirror focus-in so the delegate has normal GTK context state.
 *
 * The delegate tracks focus for its own state machine, so skipping this would
 * leave it out of step with the widget the user is typing into.
 */
static void focus_in(GtkIMContext* context)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    debug(self, "focus-in");

    gtk_im_context_focus_in(self->delegate);
}

/**
 * Mirror focus-out and let the delegate clear any native pending state.
 *
 * This is what keeps an unfinished sequence from following the user into the
 * next widget.
 */
static void focus_out(GtkIMContext* context)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    debug(self, "focus-out");

    gtk_im_context_focus_out(self->delegate);
}

/**
 * Reset delegate state when GTK asks this context to reset.
 *
 * The wrapper holds no composition state of its own, so forwarding the call is
 * the whole implementation.
 */
static void reset(GtkIMContext* context)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    debug(self, "reset");

    gtk_im_context_reset(self->delegate);
}

/**
 * Forward cursor location for clients/delegates that need surrounding context.
 *
 * GTK's public gtk_im_context_set_cursor_location() accepts const
 * GdkRectangle*, but GtkIMContextClass.set_cursor_location is declared as
 * GdkRectangle* in the GTK 4.22 public header. This vfunc must keep the
 * non-const signature or the class assignment below becomes an incompatible
 * function-pointer conversion.
 */
// NOLINTNEXTLINE(readability-non-const-parameter)
static void set_cursor_location(GtkIMContext* context, GdkRectangle* area)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    gtk_im_context_set_cursor_location(self->delegate, area);
}

/**
 * Ignore the requested value and force the delegate into no-preedit mode.
 *
 * A client that asks for preedit must still not get any from this module, so
 * the request is deliberately dropped.
 */
static void set_use_preedit(GtkIMContext* context, const gboolean use)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    (void)use;

    debug(self, "forcing delegated use-preedit=false");

    gtk_im_context_set_use_preedit(self->delegate, FALSE);
}

/**
 * Compatibility vfunc: forward via the non-deprecated selection-aware API.
 *
 * The cursor doubles as the anchor because the legacy entry point has no way
 * to express a selection.
 */
static void set_surrounding(GtkIMContext* context, const char* text, const int len, const int cursor)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    gtk_im_context_set_surrounding_with_selection(self->delegate, text, len, cursor, cursor);
}

/**
 * Compatibility vfunc: retrieve surrounding text through the modern GTK API.
 *
 * The selection anchor is discarded here because the deprecated vfunc has
 * nowhere to report it.
 */
static gboolean get_surrounding(GtkIMContext* context, char** text, int* cursor)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    int anchor = 0;

    return gtk_im_context_get_surrounding_with_selection(self->delegate, text, cursor, &anchor);
}

/**
 * Forward modern surrounding-text state to the delegate.
 *
 * This is the entry point current GTK uses, and it carries the anchor that the
 * legacy vfunc cannot express.
 */
static void set_surrounding_with_selection(GtkIMContext* context, const char* text, const int len, const int cursor, const int anchor)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    gtk_im_context_set_surrounding_with_selection(self->delegate, text, len, cursor, anchor);
}

/**
 * Forward modern surrounding-text retrieval to the delegate.
 *
 * The delegate reaches the real client back through this wrapper, so the
 * result is whatever the application supplied.
 */
static gboolean get_surrounding_with_selection(GtkIMContext* context, char** text, int* cursor, int* anchor)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    return gtk_im_context_get_surrounding_with_selection(self->delegate, text, cursor, anchor);
}

#if GTK_CHECK_VERSION(4, 14, 0)
/**
 * Forward OSK activation for GTK versions that expose this vfunc.
 *
 * The delegate's result is dropped because this variant has no way to report
 * failure back to GTK.
 */
static void activate_osk(GtkIMContext* context)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    (void)gtk_im_context_activate_osk(self->delegate, NULL);
}

/**
 * Forward event-aware OSK activation and return the delegate result.
 *
 * The event lets GTK decide whether the request came from real user input,
 * which is why the delegate's answer is passed back unchanged.
 */
static gboolean activate_osk_with_event(GtkIMContext* context, GdkEvent* event)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    return gtk_im_context_activate_osk(self->delegate, event);
}
#endif

#if GTK_CHECK_VERSION(4, 22, 0)
/**
 * Preserve GTK's default invalid-composition behavior when available.
 *
 * The parent handler runs when it exists, so this module does not change how a
 * rejected sequence looks or sounds on GTK 4.22 and newer.
 */
static gboolean invalid_composition(GtkIMContext* context, const char* str)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    debug(self, "invalid-composition default handler");

    const GtkIMContextClass* parent_class = GTK_IM_CONTEXT_CLASS(sc_im_context_parent_class);

    if (parent_class->invalid_composition != NULL)
        return parent_class->invalid_composition(context, str);

    return FALSE;
}
#endif

/**
 * Install GtkIMContext and GObject virtual methods.
 *
 * Every surrounding-text and on-screen keyboard vfunc is overridden so the
 * wrapper stays transparent, and input purpose and hints are overridden rather
 * than installed because GtkIMContext already defines them.
 */
static void sc_im_context_class_init(ScIMContextClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    GtkIMContextClass* im_context_class = GTK_IM_CONTEXT_CLASS(klass);

    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose = dispose;

    g_object_class_override_property(object_class, PROP_INPUT_PURPOSE, "input-purpose");
    g_object_class_override_property(object_class, PROP_INPUT_HINTS, "input-hints");

    im_context_class->set_client_widget = set_client_widget;
    im_context_class->get_preedit_string = get_preedit_string;
    im_context_class->filter_keypress = filter_keypress;
    im_context_class->focus_in = focus_in;
    im_context_class->focus_out = focus_out;
    im_context_class->reset = reset;
    im_context_class->set_cursor_location = set_cursor_location;
    im_context_class->set_use_preedit = set_use_preedit;
    im_context_class->set_surrounding = set_surrounding;
    im_context_class->get_surrounding = get_surrounding;
    im_context_class->set_surrounding_with_selection = set_surrounding_with_selection;
    im_context_class->get_surrounding_with_selection = get_surrounding_with_selection;
#if GTK_CHECK_VERSION(4, 14, 0)
    im_context_class->activate_osk = activate_osk;
    im_context_class->activate_osk_with_event = activate_osk_with_event;
#endif
#if GTK_CHECK_VERSION(4, 22, 0)
    im_context_class->invalid_composition = invalid_composition;
#endif
}

#ifndef SC_STATIC_TYPE
/**
 * Required by G_DEFINE_DYNAMIC_TYPE; no class-owned resources need cleanup.
 *
 * Unloading the module therefore has nothing to release beyond what the type
 * system handles itself.
 */
static void sc_im_context_class_finalize(ScIMContextClass* klass) {}
#endif

/**
 * Create the GtkIMContextSimple delegate and put it in no-preedit mode.
 *
 * The delegate is configured with the wrapper's cached purpose and hints
 * before any signal is connected, so both sides start in the same state.
 */
static void sc_im_context_init(ScIMContext* self)
{
    self->debug = debug_enabled();

    self->input_purpose = GTK_INPUT_PURPOSE_FREE_FORM;
    self->input_hints = GTK_INPUT_HINT_NONE;

    self->delegate = gtk_im_context_simple_new();

    g_object_set(self->delegate, "input-purpose", self->input_purpose, "input-hints", self->input_hints, NULL);

    gtk_im_context_set_use_preedit(self->delegate, FALSE);
    connect_delegate(self);

    debug(self, "created silent-compose GtkIMContext wrapper");
}

#ifdef SC_STATIC_TYPE
/**
 * Constructor used by unit tests that link this file statically.
 *
 * Those builds have no GIO module to instantiate the type on their behalf, so
 * the type is created directly.
 */
GtkIMContext* im_context_new(void)
{
    return g_object_new(SC_TYPE_IM_CONTEXT, NULL);
}
#endif

#ifndef SC_STATIC_TYPE
/**
 * Public wrapper around the static function generated by G_DEFINE_DYNAMIC_TYPE.
 *
 * The GIO module entry point lives in another translation unit and cannot see
 * the generated symbol without it.
 */
void im_context_register_type(GTypeModule* module)
{
    sc_im_context_register_type(module);
}
#endif
