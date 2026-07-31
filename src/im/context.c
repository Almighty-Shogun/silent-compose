#include "context.h"

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

typedef struct
{
    /* Required first field for the class struct. */
    GtkIMContextClass parent_class G_GNUC_UNUSED;
} ScIMContextClass;

#define SC_IM_CONTEXT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), SC_TYPE_IM_CONTEXT, ScIMContext))

enum
{
    PROP_INPUT_PURPOSE = 1,
    PROP_INPUT_HINTS,
};

static void sc_im_context_class_init(ScIMContextClass* klass);
#ifndef SC_STATIC_TYPE
static void sc_im_context_class_finalize(ScIMContextClass* klass);
#endif
static void sc_im_context_init(ScIMContext* self);

#ifdef SC_STATIC_TYPE
G_DEFINE_TYPE(ScIMContext, sc_im_context, GTK_TYPE_IM_CONTEXT)
#else
G_DEFINE_DYNAMIC_TYPE(ScIMContext, sc_im_context, GTK_TYPE_IM_CONTEXT)
#endif

static gboolean debug_enabled(void)
{
    const char* value = g_getenv("SILENT_COMPOSE_DEBUG");

    return value != NULL && value[0] != '\0' && g_strcmp0(value, "0") != 0;
}

/* Emit debug logs only when explicitly requested by SILENT_COMPOSE_DEBUG. */
static void debug(const ScIMContext* self, const char* message)
{
    if (self->debug)
        g_debug("%s", message);
}

/* GTK adds signals across releases; look up optional ones before connecting. */
static gboolean signal_exists(const GType type, const char* signal_name)
{
    return g_signal_lookup(signal_name, type) != 0;
}

/* Forward only completed text from GtkIMContextSimple to the client context. */
static void delegate_commit_cb(GtkIMContext* delegate, const char* str, void* const user_data)
{
    ScIMContext* self = SC_IM_CONTEXT(user_data);

    if (self->debug)
        g_debug("forwarding commit from delegated GtkIMContextSimple: “%s”", str);

    g_signal_emit_by_name(self, "commit", str);
}

/* Let the delegate ask the client for surrounding text through this wrapper. */
static gboolean delegate_retrieve_surrounding_cb(GtkIMContext* delegate, void* const user_data)
{
    ScIMContext* self = SC_IM_CONTEXT(user_data);
    gboolean handled = FALSE;

    debug(self, "forwarding retrieve-surrounding request");
    g_signal_emit_by_name(self, "retrieve-surrounding", &handled);

    return handled;
}

/* Forward deletion requests from the delegate to the real client. */
static gboolean delegate_delete_surrounding_cb(GtkIMContext* delegate, const int offset, const int n_chars, void* const user_data)
{
    ScIMContext* self = SC_IM_CONTEXT(user_data);
    gboolean handled = FALSE;

    debug(self, "forwarding delete-surrounding request");
    g_signal_emit_by_name(self, "delete-surrounding", offset, n_chars, &handled);

    return handled;
}

/* Preserve GTK's invalid-composition behavior when the signal exists. */
static gboolean delegate_invalid_composition_cb(GtkIMContext* delegate, const char* str, void* const user_data)
{
    ScIMContext* self = SC_IM_CONTEXT(user_data);
    gboolean handled = FALSE;

    if (!signal_exists(G_OBJECT_TYPE(self), "invalid-composition"))
        return FALSE;

    debug(self, "forwarding invalid-composition");
    g_signal_emit_by_name(self, "invalid-composition", str, &handled);

    return handled;
}

/*
 * Critical invariant: delegated preedit signals are intentionally swallowed.
 * Applications must never see a pending dead key, underline, highlighted range,
 * popup, or other temporary preedit UI from this module.
 */
static void delegate_preedit_ignored_cb(GtkIMContext* delegate, void* const user_data)
{
    const ScIMContext* self = SC_IM_CONTEXT(user_data);

    debug(self, "suppressed delegated preedit signal");
}

/* Connect all delegate signals used to forward commits/state and suppress preedit. */
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

/* Keep overridden input-purpose/input-hints properties mirrored to the delegate. */
static void set_property(GObject* object, const guint property_id,const GValue* value, GParamSpec* pspec)
{
    ScIMContext* self = SC_IM_CONTEXT(object);

    switch (property_id)
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
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

/* Report the wrapper's cached property values. */
static void get_property(GObject* object, const guint property_id, GValue* value, GParamSpec* pspec)
{
    const ScIMContext* self = SC_IM_CONTEXT(object);

    switch (property_id)
    {
    case PROP_INPUT_PURPOSE:
        g_value_set_enum(value, self->input_purpose);
        break;

    case PROP_INPUT_HINTS:
        g_value_set_flags(value, self->input_hints);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

/* Disconnect delegate signals before releasing the delegate context. */
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

/* Forward client-widget ownership/state to GtkIMContextSimple. */
static void  set_client_widget(GtkIMContext* context, GtkWidget* widget)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    gtk_im_context_set_client_widget(self->delegate, widget);
}

/*
 * Always return an empty preedit string.  Returning attrs with an empty string
 * keeps the API contract while preventing visible preedit UI.
 */
static void get_preedit_string(GtkIMContext* context, char** str, PangoAttrList** attrs, int* cursor_pos)
{
    if (str != NULL)
        *str = g_strdup("");

    if (attrs != NULL)
        *attrs = pango_attr_list_new();

    if (cursor_pos != NULL)
        *cursor_pos = 0;
}

/* Delegate key filtering; visible preedit output is still suppressed by signal handlers. */
static gboolean filter_keypress(GtkIMContext* context, GdkEvent* event)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);
    const gboolean handled = gtk_im_context_filter_keypress(self->delegate, event);

    if (self->debug)
        g_debug("delegated key event handled=%s", handled ? "true" : "false");

    return handled;
}

/* Mirror focus-in so the delegate has normal GTK context state. */
static void focus_in(GtkIMContext* context)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    debug(self, "focus-in");
    gtk_im_context_focus_in(self->delegate);
}

/* Mirror focus-out and let the delegate clear any native pending state. */
static void focus_out(GtkIMContext* context)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    debug(self, "focus-out");
    gtk_im_context_focus_out(self->delegate);
}

/* Reset delegate state when GTK asks this context to reset. */
static void reset(GtkIMContext* context)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    debug(self, "reset");
    gtk_im_context_reset(self->delegate);
}

/*
 * Forward cursor location for clients/delegates that need surrounding context.
 *
 * GTK's public gtk_im_context_set_cursor_location() accepts const GdkRectangle*,
 * but GtkIMContextClass.set_cursor_location is declared as GdkRectangle* in the
 * GTK 4.22 public header.  This vfunc must keep the non-const signature or the
 * class assignment below becomes an incompatible function-pointer conversion.
 */
// NOLINTNEXTLINE(readability-non-const-parameter)
static void set_cursor_location(GtkIMContext* context, GdkRectangle* area)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    gtk_im_context_set_cursor_location(self->delegate, area);
}

/* Ignore the requested value and force the delegate into no-preedit mode. */
static void set_use_preedit(GtkIMContext* context, const gboolean use_preedit)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    (void)use_preedit;

    debug(self, "forcing delegated use-preedit=false");
    gtk_im_context_set_use_preedit(self->delegate, FALSE);
}

/* Compatibility vfunc: forward via the non-deprecated selection-aware API. */
static void set_surrounding(GtkIMContext* context, const char* text, const int len, const int cursor_index)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    gtk_im_context_set_surrounding_with_selection(self->delegate, text, len, cursor_index, cursor_index);
}

/* Compatibility vfunc: retrieve surrounding text through the modern GTK API. */
static gboolean get_surrounding(GtkIMContext* context, char** text, int* cursor_index)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);
    int anchor_index = 0;

    return gtk_im_context_get_surrounding_with_selection(self->delegate, text, cursor_index, &anchor_index);
}

/* Forward modern surrounding-text state to the delegate. */
static void set_surrounding_with_selection(GtkIMContext* context, const char* text, const int len, const int cursor_index, const int anchor_index)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    gtk_im_context_set_surrounding_with_selection(self->delegate, text, len, cursor_index, anchor_index);
}

/* Forward modern surrounding-text retrieval to the delegate. */
static gboolean get_surrounding_with_selection(GtkIMContext* context, char** text, int* cursor_index, int* anchor_index)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    return gtk_im_context_get_surrounding_with_selection(self->delegate, text, cursor_index, anchor_index);
}

#if GTK_CHECK_VERSION(4, 14, 0)
/* Forward OSK activation for GTK versions that expose this vfunc. */
static void activate_osk(GtkIMContext* context)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    (void)gtk_im_context_activate_osk(self->delegate, NULL);
}

/* Forward event-aware OSK activation and return the delegate result. */
static gboolean activate_osk_with_event(GtkIMContext* context, GdkEvent* event)
{
    const ScIMContext* self = SC_IM_CONTEXT(context);

    return gtk_im_context_activate_osk(self->delegate, event);
}
#endif

#if GTK_CHECK_VERSION(4, 22, 0)
/* Preserve GTK's default invalid-composition behavior when available. */
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

/* Install GtkIMContext and GObject virtual methods. */
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
/* Required by G_DEFINE_DYNAMIC_TYPE; no class-owned resources need cleanup. */
static void sc_im_context_class_finalize(ScIMContextClass* klass) {}
#endif

/* Create the GtkIMContextSimple delegate and put it in no-preedit mode. */
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
/* Constructor used by unit tests that link this file statically. */
GtkIMContext* im_context_new(void)
{
    return g_object_new(SC_TYPE_IM_CONTEXT, NULL);
}
#endif

#ifndef SC_STATIC_TYPE
/* Public wrapper around the static function generated by G_DEFINE_DYNAMIC_TYPE. */
void im_context_register_type(GTypeModule* module)
{
    sc_im_context_register_type(module);
}
#endif
