#include "compose.h"

/**
 * Keysyms this state machine recognizes by value.
 *
 * Toolkits deliver keysyms rather than characters for editing, navigation, and
 * dead keys, so the keys that cancel or complete a composition have to be
 * matched against these constants.
 */
enum
{
    SC_KEY_BACKSPACE = 0xff08,
    SC_KEY_ESCAPE = 0xff1b,
    SC_KEY_DELETE = 0xffff,
    SC_KEY_LEFT = 0xff51,
    SC_KEY_UP = 0xff52,
    SC_KEY_RIGHT = 0xff53,
    SC_KEY_DOWN = 0xff54,
    SC_KEY_HOME = 0xff50,
    SC_KEY_END = 0xff57,
    SC_KEY_PAGE_UP = 0xff55,
    SC_KEY_PAGE_DOWN = 0xff56,
    SC_KEY_INSERT = 0xff63,
    SC_KEY_DEAD_GRAVE = 0xfe50,
    SC_KEY_DEAD_ACUTE = 0xfe51,
    SC_KEY_DEAD_CIRCUMFLEX = 0xfe52,
    SC_KEY_DEAD_TILDE = 0xfe53,
    SC_KEY_DEAD_DIAERESIS = 0xfe57,
};

/**
 * Accent currently held back from the application.
 *
 * ComposeState stores the value as a guint, which keeps this enum private to
 * the implementation while still describing what is pending.
 */
typedef enum
{
    PENDING_NONE,
    PENDING_ACUTE,
    PENDING_DIAERESIS,
    PENDING_GRAVE,
    PENDING_CIRCUMFLEX,
    PENDING_TILDE,
} PendingAccent;

/**
 * One entry of the built-in compose table.
 *
 * A rule pairs a pending accent with the printable character that completes
 * it. "output" points at static UTF-8 text, duplicated only when a commit is
 * actually produced.
 */
typedef struct
{
    PendingAccent accent;
    gunichar input;
    const char* output;
} ComposeRule;

/**
 * Built-in Windows US-International-style compose table.
 *
 * This table is deliberately small and explicit. The IBus backend covers the
 * wider Windows US-International AltGr table, while these rules keep the five
 * Silent Compose accent sequences private until the final character is ready.
 */
static const ComposeRule compose_rules[] = {
    {.accent = PENDING_ACUTE, .input = 'a', .output = "á"},
    {.accent = PENDING_ACUTE, .input = 'A', .output = "Á"},
    {.accent = PENDING_ACUTE, .input = 'c', .output = "ç"},
    {.accent = PENDING_ACUTE, .input = 'C', .output = "Ç"},
    {.accent = PENDING_ACUTE, .input = 'e', .output = "é"},
    {.accent = PENDING_ACUTE, .input = 'E', .output = "É"},
    {.accent = PENDING_ACUTE, .input = 'i', .output = "í"},
    {.accent = PENDING_ACUTE, .input = 'I', .output = "Í"},
    {.accent = PENDING_ACUTE, .input = 'o', .output = "ó"},
    {.accent = PENDING_ACUTE, .input = 'O', .output = "Ó"},
    {.accent = PENDING_ACUTE, .input = 'u', .output = "ú"},
    {.accent = PENDING_ACUTE, .input = 'U', .output = "Ú"},
    {.accent = PENDING_ACUTE, .input = 'y', .output = "ý"},
    {.accent = PENDING_ACUTE, .input = 'Y', .output = "Ý"},

    {.accent = PENDING_DIAERESIS, .input = 'a', .output = "ä"},
    {.accent = PENDING_DIAERESIS, .input = 'A', .output = "Ä"},
    {.accent = PENDING_DIAERESIS, .input = 'e', .output = "ë"},
    {.accent = PENDING_DIAERESIS, .input = 'E', .output = "Ë"},
    {.accent = PENDING_DIAERESIS, .input = 'i', .output = "ï"},
    {.accent = PENDING_DIAERESIS, .input = 'I', .output = "Ï"},
    {.accent = PENDING_DIAERESIS, .input = 'o', .output = "ö"},
    {.accent = PENDING_DIAERESIS, .input = 'O', .output = "Ö"},
    {.accent = PENDING_DIAERESIS, .input = 'u', .output = "ü"},
    {.accent = PENDING_DIAERESIS, .input = 'U', .output = "Ü"},
    {.accent = PENDING_DIAERESIS, .input = 'y', .output = "ÿ"},

    {.accent = PENDING_GRAVE, .input = 'a', .output = "à"},
    {.accent = PENDING_GRAVE, .input = 'A', .output = "À"},
    {.accent = PENDING_GRAVE, .input = 'e', .output = "è"},
    {.accent = PENDING_GRAVE, .input = 'E', .output = "È"},
    {.accent = PENDING_GRAVE, .input = 'i', .output = "ì"},
    {.accent = PENDING_GRAVE, .input = 'I', .output = "Ì"},
    {.accent = PENDING_GRAVE, .input = 'o', .output = "ò"},
    {.accent = PENDING_GRAVE, .input = 'O', .output = "Ò"},
    {.accent = PENDING_GRAVE, .input = 'u', .output = "ù"},
    {.accent = PENDING_GRAVE, .input = 'U', .output = "Ù"},

    {.accent = PENDING_CIRCUMFLEX, .input = 'a', .output = "â"},
    {.accent = PENDING_CIRCUMFLEX, .input = 'A', .output = "Â"},
    {.accent = PENDING_CIRCUMFLEX, .input = 'e', .output = "ê"},
    {.accent = PENDING_CIRCUMFLEX, .input = 'E', .output = "Ê"},
    {.accent = PENDING_CIRCUMFLEX, .input = 'i', .output = "î"},
    {.accent = PENDING_CIRCUMFLEX, .input = 'I', .output = "Î"},
    {.accent = PENDING_CIRCUMFLEX, .input = 'o', .output = "ô"},
    {.accent = PENDING_CIRCUMFLEX, .input = 'O', .output = "Ô"},
    {.accent = PENDING_CIRCUMFLEX, .input = 'u', .output = "û"},
    {.accent = PENDING_CIRCUMFLEX, .input = 'U', .output = "Û"},

    {.accent = PENDING_TILDE, .input = 'a', .output = "ã"},
    {.accent = PENDING_TILDE, .input = 'A', .output = "Ã"},
    {.accent = PENDING_TILDE, .input = 'n', .output = "ñ"},
    {.accent = PENDING_TILDE, .input = 'N', .output = "Ñ"},
    {.accent = PENDING_TILDE, .input = 'o', .output = "õ"},
    {.accent = PENDING_TILDE, .input = 'O', .output = "Õ"}
};

/**
 * Return an unhandled result without commit text.
 *
 * Building the value explicitly keeps every exit path of the state machine
 * uniform, so no caller has to tell a zeroed result apart from a real one.
 */
static ComposeResult result_unhandled(void)
{
    return (ComposeResult){
        .handled = FALSE,
        .commit = NULL
    };
}

/**
 * Return a handled result and duplicate commit text for the caller.
 *
 * A NULL commit means "consume this key silently". Anything else is copied
 * here because the compose table owns static strings the caller must not free.
 */
static ComposeResult result_handled(const char* commit)
{
    return (ComposeResult){
        .handled = TRUE,
        .commit = commit != NULL ? g_strdup(commit) : NULL,
    };
}

/**
 * Convert the incoming key to a Unicode scalar when it represents printable text.
 *
 * This accepts ASCII, Latin-1 keysyms, and the GDK/IBus Unicode keysyms of the
 * form 0x01000000 + codepoint, which is what layouts emit for characters
 * outside the legacy keysym ranges.
 */
static gboolean key_to_unicode(const guint key, gunichar* out)
{
    if (key < 0x80)
    {
        *out = key;

        return g_unichar_validate(*out);
    }

    if (key >= 0x01000100 && key <= 0x0110ffff)
    {
        *out = key - 0x01000000;

        return g_unichar_validate(*out);
    }

    if (key >= 0x00a0 && key <= 0x00ff)
    {
        *out = key;

        return g_unichar_validate(*out);
    }

    return FALSE;
}

/**
 * Convert one validated Unicode scalar to a newly allocated UTF-8 string.
 *
 * Validation happens here rather than at the call sites, so a malformed keysym
 * can never reach a client as commit text.
 */
static char* utf8_from_unichar(const gunichar ch)
{
    char buf[7] = {0};

    if (!g_unichar_validate(ch))
        return NULL;

    const int len = g_unichar_to_utf8(ch, buf);

    buf[len] = '\0';

    return g_strdup(buf);
}

/**
 * Report whether the modifiers turn this key into an application shortcut.
 *
 * Shortcuts must pass through to applications unchanged. This is why Ctrl+C,
 * Ctrl+V, Alt shortcuts, and Super shortcuts are not swallowed by the input
 * method, even if an accent is currently pending.
 */
static gboolean is_modifier_shortcut(const guint mods)
{
    return (mods & (COMPOSE_MOD_CONTROL | COMPOSE_MOD_ALT | COMPOSE_MOD_SUPER)) != 0;
}

/**
 * Report whether the key is navigation or function input rather than text.
 *
 * If one of these arrives while an accent is pending, the pending literal is
 * committed but the result stays unhandled, so the caller can still deliver
 * the navigation key normally.
 */
static gboolean is_non_printable_navigation_key(const guint key)
{
    return key == SC_KEY_DELETE
           || key == SC_KEY_LEFT
           || key == SC_KEY_UP
           || key == SC_KEY_RIGHT
           || key == SC_KEY_DOWN
           || key == SC_KEY_HOME
           || key == SC_KEY_END
           || key == SC_KEY_PAGE_UP
           || key == SC_KEY_PAGE_DOWN
           || key == SC_KEY_INSERT
           || (key >= 0xffbe && key <= 0xffe0);
}

/**
 * Detect the five accent keys handled by this state machine.
 *
 * Printable accent characters and XKB dead-key keysyms map to the same accent,
 * and the exact literal is remembered for the Space, repeated accent, and
 * fallback cases.
 */
static gboolean pending_from_key(const guint key, PendingAccent* accent, gunichar* literal)
{
    switch (key)
    {
        case '\'':
        case SC_KEY_DEAD_ACUTE:
            *accent = PENDING_ACUTE;
            *literal = '\'';
            return TRUE;

        case '"':
        case SC_KEY_DEAD_DIAERESIS:
            *accent = PENDING_DIAERESIS;
            *literal = '"';
            return TRUE;

        case '`':
        case SC_KEY_DEAD_GRAVE:
            *accent = PENDING_GRAVE;
            *literal = '`';
            return TRUE;

        case '^':
        case SC_KEY_DEAD_CIRCUMFLEX:
            *accent = PENDING_CIRCUMFLEX;
            *literal = '^';
            return TRUE;

        case '~':
        case SC_KEY_DEAD_TILDE:
            *accent = PENDING_TILDE;
            *literal = '~';
            return TRUE;

        default:
            return FALSE;
    }
}

/**
 * Look up a completed accent and printable-character sequence.
 *
 * The table is short enough that a linear scan costs less than any index, and
 * the returned string is static storage owned by compose_rules.
 */
static const char* lookup_composition(const PendingAccent accent, const gunichar input)
{
    for (gsize i = 0; i < G_N_ELEMENTS(compose_rules); i++)
    {
        if (compose_rules[i].accent == accent && compose_rules[i].input == input)
            return compose_rules[i].output;
    }

    return NULL;
}

/**
 * Build fallback text such as "'t" or "~~" when no compose rule exists.
 *
 * Preserving both characters is what keeps an unsupported sequence from losing
 * the keystrokes the user already typed.
 */
static char* concat_unichars(const gunichar first, const gunichar second)
{
    char* first_utf8 = utf8_from_unichar(first);
    char* second_utf8 = utf8_from_unichar(second);

    if (first_utf8 == NULL || second_utf8 == NULL)
    {
        g_free(first_utf8);
        g_free(second_utf8);

        return NULL;
    }

    char* combined = g_strconcat(first_utf8, second_utf8, NULL);

    g_free(first_utf8);
    g_free(second_utf8);

    return combined;
}

/**
 * Allocate composition state with no pending accent.
 *
 * Zeroed memory is already the idle state because PENDING_NONE is the first
 * enumerator, so nothing else has to be initialized.
 */
ComposeState* compose_state_new(void)
{
    return g_new0(ComposeState, 1);
}

/**
 * Release composition state allocated by compose_state_new().
 *
 * The state owns no allocations of its own, so one free is enough and a NULL
 * state is tolerated exactly the way g_free() tolerates it.
 */
void compose_state_free(ComposeState* state)
{
    g_free(state);
}

/**
 * Clear pending accent state without producing committed text.
 *
 * The literal is cleared alongside the accent, so a later fallback can never
 * commit a character left over from an abandoned sequence.
 */
void compose_state_reset(ComposeState* state)
{
    g_return_if_fail(state != NULL);

    state->pending = PENDING_NONE;
    state->pending_literal = 0;
}

/**
 * Report whether an accent key is currently stored in private state.
 *
 * The IBus backend checks this before its own AltGr passthrough, which keeps a
 * pending accent ahead of any direct symbol commit.
 */
gboolean compose_state_is_pending(const ComposeState* state)
{
    g_return_val_if_fail(state != NULL, FALSE);

    return state->pending != PENDING_NONE;
}

/**
 * Process one normalized key event and decide what the caller must do.
 *
 * The three outcomes are: leave the key alone, consume it to start or cancel a
 * sequence, or return completed UTF-8 text. Release events, shortcuts, and
 * navigation keys are routed out before any accent handling happens.
 */
ComposeResult compose_state_process_key(ComposeState* state, const guint key, const guint mods)
{
    g_return_val_if_fail(state != NULL, result_unhandled ());

    if ((mods & COMPOSE_MOD_RELEASE) != 0)
        return result_unhandled();

    /*
     * No pending accent: only consume one of the accent keys. Ordinary text
     * and shortcuts stay unhandled so the application/toolkit receives them
     * exactly as usual.
     */
    if (state->pending == PENDING_NONE)
    {
        if (is_modifier_shortcut(mods))
            return result_unhandled();

        gunichar literal;
        PendingAccent accent;

        if (pending_from_key(key, &accent, &literal))
        {
            state->pending = accent;
            state->pending_literal = literal;

            return result_handled(NULL);
        }

        return result_unhandled();
    }

    /*
     * Escape and Backspace cancel an unfinished sequence. They are consumed so
     * the application does not delete text or receive Escape for a sequence that
     * existed only inside the input method.
     */
    if (key == SC_KEY_ESCAPE || key == SC_KEY_BACKSPACE)
    {
        compose_state_reset(state);

        return result_handled(NULL);
    }

    if (is_modifier_shortcut(mods))
        return result_unhandled();

    gunichar input;

    if (key_to_unicode(key, &input))
    {
        /*
         * Accent + Space means "commit one literal accent". Repeating the same
         * accent intentionally does not use this path; it falls through to the
         * generic fallback and commits two literal characters.
         */
        if (input == ' ')
        {
            char* commit = utf8_from_unichar(state->pending_literal);

            const ComposeResult result = {
                .handled = TRUE,
                .commit = commit
            };

            compose_state_reset(state);

            return result;
        }

        const char* composed = lookup_composition(state->pending, input);

        if (composed != NULL)
        {
            const ComposeResult result = result_handled(composed);

            compose_state_reset(state);

            return result;
        }

        /*
         * Unsupported printable sequence fallback.  This preserves input by
         * committing the pending literal plus the new printable character.
         */
        char* fallback = concat_unichars(state->pending_literal, input);

        compose_state_reset(state);

        if (fallback != NULL)
            return (ComposeResult){
                .handled = TRUE,
                .commit = fallback
            };

        return result_unhandled();
    }

    if (is_non_printable_navigation_key(key))
    {
        char* commit = utf8_from_unichar(state->pending_literal);

        const ComposeResult result = {
            .handled = FALSE,
            .commit = commit
        };

        compose_state_reset(state);

        return result;
    }

    return result_unhandled();
}

/**
 * Free commit text owned by the result and reset it to the unhandled state.
 *
 * Callers run this after every event, so tolerating a NULL result and an
 * already cleared one keeps the call sites free of guards.
 */
void compose_result_clear(ComposeResult* result)
{
    if (result == NULL)
        return;

    g_clear_pointer(&result->commit, g_free);

    result->handled = FALSE;
}
