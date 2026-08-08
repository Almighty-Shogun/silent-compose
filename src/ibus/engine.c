#include "engine.h"
#include "compose.h"

#include <stdarg.h>
#include <stdio.h>

typedef struct
{
    /*
     * Required first field for a GObject subtype. It is read by the type
     * system through casts/macros, not directly by our code.
     */
    IBusEngine parent_instance G_GNUC_UNUSED;

    ComposeState* compose;
    gboolean debug;
    gboolean altgr;
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

typedef struct
{
    guint code;
    guint key;
    guint shifted;
    gunichar altgr;
    gunichar altgr_shift;
} WinIntlSymbol;

/*
 * Printable AltGr symbols from Microsoft's KBDUSX United States-International
 * layout.  IBus passes evdev keycodes, which are 8 lower than the XKB keycodes
 * shown by tools such as wev.
 */
static const WinIntlSymbol win_intl_symbols[] = {
    { .code = 2, .key = '1', .shifted = '!', .altgr = 0x00a1, .altgr_shift = 0x00b9 },
    { .code = 3, .key = '2', .shifted = '@', .altgr = 0x00b2 },
    { .code = 4, .key = '3', .shifted = '#', .altgr = 0x00b3 },
    { .code = 5, .key = '4', .shifted = '$', .altgr = 0x00a4, .altgr_shift = 0x00a3 },
    { .code = 6, .key = '5', .shifted = '%', .altgr = 0x20ac },
    { .code = 7, .key = '6', .shifted = '^', .altgr = 0x00bc },
    { .code = 8, .key = '7', .shifted = '&', .altgr = 0x00bd },
    { .code = 9, .key = '8', .shifted = '*', .altgr = 0x00be },
    { .code = 10, .key = '9', .shifted = '(', .altgr = 0x2018 },
    { .code = 11, .key = '0', .shifted = ')', .altgr = 0x2019 },
    { .code = 12, .key = '-', .shifted = '_', .altgr = 0x00a5 },
    { .code = 13, .key = '=', .shifted = '+', .altgr = 0x00d7, .altgr_shift = 0x00f7 },
    { .code = 16, .key = 'q', .shifted = 'Q', .altgr = 0x00e4, .altgr_shift = 0x00c4 },
    { .code = 17, .key = 'w', .shifted = 'W', .altgr = 0x00e5, .altgr_shift = 0x00c5 },
    { .code = 18, .key = 'e', .shifted = 'E', .altgr = 0x00e9, .altgr_shift = 0x00c9 },
    { .code = 19, .key = 'r', .shifted = 'R', .altgr = 0x00ae },
    { .code = 20, .key = 't', .shifted = 'T', .altgr = 0x00fe, .altgr_shift = 0x00de },
    { .code = 21, .key = 'y', .shifted = 'Y', .altgr = 0x00fc, .altgr_shift = 0x00dc },
    { .code = 22, .key = 'u', .shifted = 'U', .altgr = 0x00fa, .altgr_shift = 0x00da },
    { .code = 23, .key = 'i', .shifted = 'I', .altgr = 0x00ed, .altgr_shift = 0x00cd },
    { .code = 24, .key = 'o', .shifted = 'O', .altgr = 0x00f3, .altgr_shift = 0x00d3 },
    { .code = 25, .key = 'p', .shifted = 'P', .altgr = 0x00f6, .altgr_shift = 0x00d6 },
    { .code = 26, .key = '[', .shifted = '{', .altgr = 0x00ab },
    { .code = 27, .key = ']', .shifted = '}', .altgr = 0x00bb },
    { .code = 30, .key = 'a', .shifted = 'A', .altgr = 0x00e1, .altgr_shift = 0x00c1 },
    { .code = 31, .key = 's', .shifted = 'S', .altgr = 0x00df, .altgr_shift = 0x00a7 },
    { .code = 32, .key = 'd', .shifted = 'D', .altgr = 0x00f0, .altgr_shift = 0x00d0 },
    { .code = 38, .key = 'l', .shifted = 'L', .altgr = 0x00f8, .altgr_shift = 0x00d8 },
    { .code = 39, .key = ';', .shifted = ':', .altgr = 0x00b6, .altgr_shift = 0x00b0 },
    { .code = 40, .key = '\'', .shifted = '"', .altgr = 0x00b4, .altgr_shift = 0x00a8 },
    { .code = 43, .key = '\\', .shifted = '|', .altgr = 0x00ac, .altgr_shift = 0x00a6 },
    { .code = 44, .key = 'z', .shifted = 'Z', .altgr = 0x00e6, .altgr_shift = 0x00c6 },
    { .code = 46, .key = 'c', .shifted = 'C', .altgr = 0x00a9, .altgr_shift = 0x00a2 },
    { .code = 49, .key = 'n', .shifted = 'N', .altgr = 0x00f1, .altgr_shift = 0x00d1 },
    { .code = 50, .key = 'm', .shifted = 'M', .altgr = 0x00b5 },
    { .code = 51, .key = ',', .shifted = '<', .altgr = 0x00e7, .altgr_shift = 0x00c7 },
    { .code = 53, .key = '/', .shifted = '?', .altgr = 0x00bf }
};

/* Return TRUE when verbose engine logging was requested through the environment. */
static gboolean debug_enabled(void)
{
    const char* value = g_getenv("SILENT_COMPOSE_DEBUG");

    return value != NULL && value[0] != '\0' && g_strcmp0(value, "0") != 0;
}

/* Append diagnostics somewhere visible even when IBus redirects stderr. */
static void debug_log(ScEngine* self, const char* format, ...)
{
    if (!self->debug)
        return;

    va_list args;

    va_start(args, format);

    char* message = g_strdup_vprintf(format, args);

    va_end(args);

    g_debug("%s", message);

    const char* path = g_getenv("SILENT_COMPOSE_DEBUG_LOG");

    if (path == NULL || path[0] == '\0')
        path = "/tmp/silent-compose-ibus.log";

    FILE* file = fopen(path, "a");

    if (file != NULL)
    {
        fprintf(file, "%s\n", message);
        fclose(file);
    }

    g_free(message);
}

/*
 * Translate IBus modifier state to the backend-neutral compose modifier bits.
 * The compose state deliberately treats Ctrl/Alt/Super as shortcut mods
 * and leaves those events to the application.
 */
static guint translate_mods(const guint state)
{
    guint mods = 0;

    if ((state & IBUS_SHIFT_MASK) != 0)
        mods |= COMPOSE_MOD_SHIFT;

    if ((state & IBUS_CONTROL_MASK) != 0)
        mods |= COMPOSE_MOD_CONTROL;

    if ((state & IBUS_MOD1_MASK) != 0)
        mods |= COMPOSE_MOD_ALT;

    if ((state & (IBUS_SUPER_MASK | IBUS_MOD4_MASK)) != 0)
        mods |= COMPOSE_MOD_SUPER;

    if ((state & IBUS_RELEASE_MASK) != 0)
        mods |= COMPOSE_MOD_RELEASE;

    return mods;
}

/* Detect modifier key events that start or end an AltGr/Level-3 session. */
static gboolean is_altgr_modifier_key(const guint key)
{
    return key == IBUS_KEY_Alt_R || key == IBUS_KEY_ISO_Level3_Shift || key == IBUS_KEY_Mode_switch;
}

/* Update the engine-local AltGr session and report whether this was its key. */
static gboolean update_altgr_session(ScEngine* self, const guint key, const guint state)
{
    if (!is_altgr_modifier_key(key))
        return FALSE;

    self->altgr = (state & IBUS_RELEASE_MASK) == 0;

    return TRUE;
}

/* Report detailed key routing when SILENT_COMPOSE_DEBUG is enabled. */
static void log_key(
    ScEngine* self,
    const char* stage,
    const guint key,
    const guint code,
    const guint state,
    const gboolean altgr_event)
{
    debug_log(self,
              "%s keyval=0x%x keycode=%u state=0x%x altgr_event=%s altgr=%s",
              stage,
              key,
              code,
              state,
              altgr_event ? "true" : "false",
              self->altgr ? "true" : "false");
}

/* Return the printable Windows US-International symbol for an AltGr key event. */
static gboolean win_intl_lookup(const guint key, const guint code, const guint state, const gboolean altgr, gunichar* out)
{
    if ((state & IBUS_MOD5_MASK) == 0 && !altgr)
        return FALSE;

    const gboolean shifted = (state & IBUS_SHIFT_MASK) != 0;

    for (gsize i = 0; i < G_N_ELEMENTS(win_intl_symbols); i++)
    {
        const WinIntlSymbol* symbol = &win_intl_symbols[i];

        if (code != 0 && code != symbol->code)
            continue;

        if (code == 0 && key != symbol->key && key != symbol->shifted)
            continue;

        *out = shifted ? symbol->altgr_shift : symbol->altgr;

        return *out != 0;
    }

    if (code == 0)
    {
        const gunichar ch = ibus_keyval_to_unicode(key);

        for (gsize i = 0; i < G_N_ELEMENTS(win_intl_symbols); i++)
        {
            const WinIntlSymbol* symbol = &win_intl_symbols[i];

            if (ch == symbol->altgr || ch == symbol->altgr_shift)
            {
                *out = ch;

                return TRUE;
            }
        }
    }

    return FALSE;
}

/* Write one validated non-ASCII printable Unicode scalar into buf. */
static gboolean write_passthrough_char(const gunichar ch, char* buf, const gsize buf_size)
{
    if (ch < 0x80 || !g_unichar_validate(ch) || g_unichar_iscntrl(ch))
        return FALSE;

    char utf8[7] = {0};
    const int len = g_unichar_to_utf8(ch, utf8);

    utf8[len] = '\0';

    if (len <= 0 || (gsize) len >= buf_size)
        return FALSE;

    g_strlcpy(buf, utf8, buf_size);

    return TRUE;
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

/* Commit raw or translated AltGr printable text when it is not part of compose. */
static gboolean commit_passthrough(IBusEngine* engine, const guint key, const guint code, const guint state, const gboolean altgr)
{
    char passthrough[7] = {0};

    if (!sc_engine_passthrough_text(key, code, state, altgr, passthrough, sizeof passthrough))
        return FALSE;

    commit_string(engine, passthrough);

    return TRUE;
}

/* Return TRUE when an engine-local AltGr chord should not reach the client. */
static gboolean consume_altgr_chord(const gboolean altgr, const guint state)
{
    if (!altgr)
        return FALSE;

    const guint blocked = IBUS_CONTROL_MASK | IBUS_SUPER_MASK | IBUS_MOD4_MASK;

    return (state & blocked) == 0;
}

/*
 * Build commit text for Windows US-International AltGr characters that
 * ComposeState does not handle itself.  Shortcuts and normal ASCII input still
 * pass through to the application.
 */
gboolean sc_engine_passthrough_text(
    const guint key,
    const guint code,
    const guint state,
    const gboolean altgr,
    char* buf,
    const gsize buf_size)
{
    g_return_val_if_fail(buf != NULL, FALSE);

    if ((state & IBUS_RELEASE_MASK) != 0)
        return FALSE;

    const guint blocked = IBUS_CONTROL_MASK | IBUS_SUPER_MASK | IBUS_MOD4_MASK;

    if ((state & blocked) != 0)
        return FALSE;

    if ((state & IBUS_MOD1_MASK) != 0 && (state & IBUS_MOD5_MASK) == 0 && !altgr)
        return FALSE;

    gunichar ch;

    if (win_intl_lookup(key, code, state, altgr, &ch))
        return write_passthrough_char(ch, buf, buf_size);

    if ((state & IBUS_MOD5_MASK) != 0 || altgr)
        return FALSE;

    ch = ibus_keyval_to_unicode(key);

    return write_passthrough_char(ch, buf, buf_size);
}

/*
 * IBus calls this for each key event.  Return TRUE only when the input method
 * consumed the key.  A handled result with commit == NULL means the key was an
 * accent, Escape, or Backspace that only affected private state.
 */
static gboolean process_key_event(IBusEngine* engine, const guint key, const guint code, const guint state)
{
    ScEngine* self = SC_ENGINE(engine);

    const gboolean altgr_event = update_altgr_session(self, key, state);

    log_key(self, "input", key, code, state, altgr_event);

    if (altgr_event)
        return TRUE;

    if (!compose_state_is_pending(self->compose) && commit_passthrough(engine, key, code, state, self->altgr))
    {
        log_key(self, "passthrough", key, code, state, altgr_event);

        return TRUE;
    }

    ComposeResult result = compose_state_process_key(self->compose, key, translate_mods(state));

    const gboolean pending = compose_state_is_pending(self->compose);

    debug_log(self,
              "compose keyval=0x%x keycode=%u state=0x%x handled=%s commit=%s pending=%s",
              key,
              code,
              state,
              result.handled ? "true" : "false",
              result.commit != NULL ? "yes" : "no",
              pending ? "true" : "false");

    const gboolean handled = result.handled;

    commit_string(engine, result.commit);
    compose_result_clear(&result);

    if (!handled && !pending)
    {
        if (commit_passthrough(engine, key, code, state, self->altgr))
        {
            log_key(self, "passthrough", key, code, state, altgr_event);

            return TRUE;
        }

        if (consume_altgr_chord(self->altgr, state))
        {
            log_key(self, "altgr-consume", key, code, state, altgr_event);

            return TRUE;
        }
    }

    return handled;
}

/* Reset pending accent state when IBus asks the engine to reset. */
static void reset(IBusEngine* engine)
{
    ScEngine* self = SC_ENGINE(engine);

    compose_state_reset(self->compose);

    self->altgr = FALSE;

    debug_log(self, "reset");

    if (IBUS_ENGINE_CLASS(sc_engine_parent_class)->reset != NULL)
        IBUS_ENGINE_CLASS(sc_engine_parent_class)->reset(engine);
}

/* Focus changes must not carry a half-finished accent into another widget. */
static void focus_out(IBusEngine* engine)
{
    ScEngine* self = SC_ENGINE(engine);

    compose_state_reset(self->compose);

    self->altgr = FALSE;

    debug_log(self, "focus-out");

    if (IBUS_ENGINE_CLASS(sc_engine_parent_class)->focus_out != NULL)
        IBUS_ENGINE_CLASS(sc_engine_parent_class)->focus_out(engine);
}

/* Disable behaves like reset: no pending state may survive engine deactivation. */
static void disable(IBusEngine* engine)
{
    ScEngine* self = SC_ENGINE(engine);

    compose_state_reset(self->compose);

    self->altgr = FALSE;

    debug_log(self, "disable");

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
    self->altgr = FALSE;
}
