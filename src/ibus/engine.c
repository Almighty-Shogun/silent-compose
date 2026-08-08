#include "engine.h"
#include "compose.h"

typedef struct
{
    /*
     * Required first field for a GObject subtype. It is read by the type
     * system through casts/macros, not directly by our code.
     */
    IBusEngine parent_instance G_GNUC_UNUSED;

    ComposeState* compose;
    gboolean debug;
} ScEngine;

typedef struct
{
    /*
     * Required first field for the class struct so GObject can treat this as
     * an IBusEngineClass.
     */
    IBusEngineClass parent_class G_GNUC_UNUSED;
} ScEngineClass;

#define SC_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SC_TYPE_ENGINE, ScEngine))

G_DEFINE_TYPE(ScEngine, sc_engine, IBUS_TYPE_ENGINE)

/* Return TRUE when verbose engine logging was requested through the environment. */
static gboolean debug_enabled(void)
{
    const char* value = g_getenv("SILENT_COMPOSE_DEBUG");

    return value != NULL && value[0] != '\0' && g_strcmp0(value, "0") != 0;
}

/*
 * Translate IBus modifier state to the backend-neutral compose modifier bits.
 * The compose state deliberately treats Ctrl/Alt/Super as shortcut modifiers
 * and leaves those events to the application.
 */
static guint translate_modifiers(const guint ibus_state)
{
    guint modifiers = 0;

    if ((ibus_state & IBUS_SHIFT_MASK) != 0)
        modifiers |= COMPOSE_MOD_SHIFT;

    if ((ibus_state & IBUS_CONTROL_MASK) != 0)
        modifiers |= COMPOSE_MOD_CONTROL;

    if ((ibus_state & IBUS_MOD1_MASK) != 0)
        modifiers |= COMPOSE_MOD_ALT;

    if ((ibus_state & (IBUS_SUPER_MASK | IBUS_MOD4_MASK)) != 0)
        modifiers |= COMPOSE_MOD_SUPER;

    if ((ibus_state & IBUS_RELEASE_MASK) != 0)
        modifiers |= COMPOSE_MOD_RELEASE;

    return modifiers;
}

/*
 * Commit completed text to IBus.
 *
 * This is the only place the IBus backend sends text to clients.  There are no
 * preedit, auxiliary text, lookup table, or candidate-window calls anywhere in
 * this engine; pending accents remain private in ComposeState.
 */
static void commit_string(IBusEngine* engine, const char* str)
{
    if (str == NULL || str[0] == '\0')
        return;

    if (!g_utf8_validate(str, -1, NULL))
        return;

    IBusText* text = ibus_text_new_from_string(str);

    if (g_object_is_floating(text))
        g_object_ref_sink(text);

    ibus_engine_commit_text(engine, text);
    g_object_unref(text);
}

/*
 * Build commit text for printable non-ASCII keysyms that ComposeState does not
 * handle itself.  This covers Level-3/AltGr characters such as EuroSign from
 * the advertised us(altgr-intl) layout while still letting shortcuts and normal
 * ASCII input pass through to the application.
 */
gboolean sc_engine_make_passthrough_text(const guint keyval, const guint state, char* buffer, const gsize buffer_size)
{
    g_return_val_if_fail(buffer != NULL, FALSE);

    if ((state & IBUS_RELEASE_MASK) != 0)
        return FALSE;

    if ((state & (IBUS_CONTROL_MASK | IBUS_SUPER_MASK | IBUS_MOD4_MASK)) != 0)
        return FALSE;

    if ((state & IBUS_MOD1_MASK) != 0 && (state & IBUS_MOD5_MASK) == 0)
        return FALSE;

    const gunichar ch = ibus_keyval_to_unicode(keyval);

    if (ch < 0x80 || !g_unichar_validate(ch) || g_unichar_iscntrl(ch))
        return FALSE;

    char utf8[7] = {0};
    const int len = g_unichar_to_utf8(ch, utf8);

    utf8[len] = '\0';

    if (len <= 0 || (gsize) len >= buffer_size)
        return FALSE;

    g_strlcpy(buffer, utf8, buffer_size);

    return TRUE;
}

/*
 * IBus calls this for each key event.  Return TRUE only when the input method
 * consumed the key.  A handled result with commit == NULL means the key was an
 * accent, Escape, or Backspace that only affected private state.
 */
static gboolean process_key_event(IBusEngine* engine, const guint keyval, const guint keycode, const guint state)
{
    const ScEngine* self = SC_ENGINE(engine);

    ComposeResult result = compose_state_process_key(self->compose, keyval, translate_modifiers(state));

    const gboolean pending = compose_state_is_pending(self->compose);

    if (self->debug)
    {
        g_debug("keyval=0x%x keycode=%u state=0x%x handled=%s commit=%s pending=%s",
                keyval,
                keycode,
                state,
                result.handled ? "true" : "false",
                result.commit != NULL ? "yes" : "no",
                pending ? "true" : "false");
    }

    const gboolean handled = result.handled;

    commit_string(engine, result.commit);
    compose_result_clear(&result);

    if (!handled && !pending)
    {
        char passthrough[7] = {0};

        if (sc_engine_make_passthrough_text(keyval, state, passthrough, sizeof passthrough))
        {
            commit_string(engine, passthrough);

            return TRUE;
        }
    }

    return handled;
}

/* Reset pending accent state when IBus asks the engine to reset. */
static void reset(IBusEngine* engine)
{
    const ScEngine* self = SC_ENGINE(engine);

    compose_state_reset(self->compose);

    if (self->debug)
        g_debug("reset");

    if (IBUS_ENGINE_CLASS(sc_engine_parent_class)->reset != NULL)
        IBUS_ENGINE_CLASS(sc_engine_parent_class)->reset(engine);
}

/* Focus changes must not carry a half-finished accent into another widget. */
static void focus_out(IBusEngine* engine)
{
    const ScEngine* self = SC_ENGINE(engine);

    compose_state_reset(self->compose);

    if (self->debug)
        g_debug("focus-out");

    if (IBUS_ENGINE_CLASS(sc_engine_parent_class)->focus_out != NULL)
        IBUS_ENGINE_CLASS(sc_engine_parent_class)->focus_out(engine);
}

/* Disable behaves like reset: no pending state may survive engine deactivation. */
static void disable(IBusEngine* engine)
{
    const ScEngine* self = SC_ENGINE(engine);

    compose_state_reset(self->compose);

    if (self->debug)
        g_debug("disable");

    if (IBUS_ENGINE_CLASS(sc_engine_parent_class)->disable != NULL)
        IBUS_ENGINE_CLASS(sc_engine_parent_class)->disable(engine);
}

/* Release the private composition state owned by this engine instance. */
static void finalize(GObject* object)
{
    ScEngine* self = SC_ENGINE(object);

    g_clear_pointer(&self->compose, compose_state_free);

    G_OBJECT_CLASS(sc_engine_parent_class)->finalize(object);
}

/* Install IBusEngine virtual methods. */
static void sc_engine_class_init(ScEngineClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    IBusEngineClass* engine_class = IBUS_ENGINE_CLASS(klass);

    object_class->finalize = finalize;

    engine_class->process_key_event = process_key_event;
    engine_class->reset = reset;
    engine_class->focus_out = focus_out;
    engine_class->disable = disable;
}

/* Each IBusEngine instance owns independent pending composition state. */
static void sc_engine_init(ScEngine* self)
{
    self->compose = compose_state_new();
    self->debug = debug_enabled();
}
