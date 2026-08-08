#include "compose.h"

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
};

typedef enum
{
    PENDING_NONE,
    PENDING_ACUTE,
    PENDING_DIAERESIS,
    PENDING_GRAVE,
    PENDING_CIRCUMFLEX,
    PENDING_TILDE,
} PendingAccent;

typedef struct
{
    PendingAccent accent;
    gunichar input;
    const char* output;
} ComposeRule;

/*
 * Built-in Windows US-International-style compose table.
 *
 * This table is deliberately small and explicit. The IBus backend advertises
 * the US altgr-intl layout variant for Level-3 characters, while these rules
 * keep the five Silent Compose accent sequences private until the final
 * character is ready.
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
    {.accent = PENDING_TILDE, .input = 'O', .output = "Õ"},
};

static ComposeResult result_unhandled(void)
{
    return (ComposeResult){
        .handled = FALSE,
        .commit = NULL
    };
}

/*
 * Return a handled result and duplicate commit text for the caller.
 * commit == NULL means "consume this key silently".
 */
static ComposeResult result_handled(const char* commit)
{
    return (ComposeResult){
        .handled = TRUE,
        .commit = commit != NULL ? g_strdup(commit) : NULL,
    };
}

/*
 * Convert the incoming keyval to a Unicode scalar when it represents printable
 * text.  This accepts ASCII, Latin-1 keysyms, and GDK/IBus Unicode keysyms of
 * the form 0x01000000 + codepoint.
 */
static gboolean keyval_to_unicode(const guint keyval, gunichar* out_char)
{
    if (keyval < 0x80)
    {
        *out_char = keyval;

        return g_unichar_validate(*out_char);
    }

    if (keyval >= 0x01000100 && keyval <= 0x0110ffff)
    {
        *out_char = keyval - 0x01000000;

        return g_unichar_validate(*out_char);
    }

    if (keyval >= 0x00a0 && keyval <= 0x00ff)
    {
        *out_char = keyval;

        return g_unichar_validate(*out_char);
    }

    return FALSE;
}

/* Convert one validated Unicode scalar to a newly allocated UTF-8 string. */
static char* utf8_from_unichar(const gunichar ch)
{
    char buffer[7] = {0};

    if (!g_unichar_validate(ch))
        return NULL;

    const int len = g_unichar_to_utf8(ch, buffer);

    buffer[len] = '\0';

    return g_strdup(buffer);
}

/*
 * Shortcuts must pass through to applications unchanged.  This is why Ctrl+C,
 * Ctrl+V, Alt shortcuts, Super shortcuts, etc. are not swallowed by the input
 * method, even if an accent is currently pending.
 */
static gboolean is_modifier_shortcut(const guint modifiers)
{
    return (modifiers & (COMPOSE_MOD_CONTROL | COMPOSE_MOD_ALT | COMPOSE_MOD_SUPER)) != 0;
}

/*
 * Navigation and function-like keys are not printable text.  If one arrives
 * while an accent is pending, we commit the pending literal but return
 * handled=FALSE so the caller can still deliver the navigation key normally.
 */
static gboolean is_non_printable_navigation_key(const guint keyval)
{
    return keyval == SC_KEY_DELETE
        || keyval == SC_KEY_LEFT
        || keyval == SC_KEY_UP
        || keyval == SC_KEY_RIGHT
        || keyval == SC_KEY_DOWN
        || keyval == SC_KEY_HOME
        || keyval == SC_KEY_END
        || keyval == SC_KEY_PAGE_UP
        || keyval == SC_KEY_PAGE_DOWN
        || keyval == SC_KEY_INSERT
        || (keyval >= 0xffbe && keyval <= 0xffe0);
}

/*
 * Detect the five accent keys handled by this state machine and remember the
 * exact literal character to use for Space, repeated accent, and fallback cases.
 */
static gboolean pending_from_keyval(const guint keyval, PendingAccent* accent, gunichar* literal)
{
    switch (keyval)
    {
        case '\'':
            *accent = PENDING_ACUTE;
            *literal = '\'';
        return TRUE;

        case '"':
            *accent = PENDING_DIAERESIS;
            *literal = '"';
        return TRUE;

        case '`':
            *accent = PENDING_GRAVE;
            *literal = '`';
        return TRUE;

        case '^':
            *accent = PENDING_CIRCUMFLEX;
            *literal = '^';
        return TRUE;

        case '~':
            *accent = PENDING_TILDE;
            *literal = '~';
        return TRUE;

    default:
        return FALSE;
    }
}

/* Look up a completed accent + printable-character sequence. */
static const char* lookup_composition(const PendingAccent accent, const gunichar input)
{
    for (gsize i = 0; i < G_N_ELEMENTS(compose_rules); i++)
    {
        if (compose_rules[i].accent == accent && compose_rules[i].input == input)
            return compose_rules[i].output;
    }

    return NULL;
}

/* Build fallback text such as "'t" or "~~" when no compose rule exists. */
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

ComposeState* compose_state_new(void)
{
    return g_new0(ComposeState, 1);
}

void compose_state_free(ComposeState* state)
{
    g_free(state);
}

void compose_state_reset(ComposeState* state)
{
    g_return_if_fail(state != NULL);

    state->pending = PENDING_NONE;
    state->pending_literal = 0;
}

gboolean compose_state_is_pending(const ComposeState* state)
{
    g_return_val_if_fail(state != NULL, FALSE);

    return state->pending != PENDING_NONE;
}

ComposeResult compose_state_process_key(ComposeState* state, const guint keyval, const guint modifiers)
{
    g_return_val_if_fail(state != NULL, result_unhandled ());

    if ((modifiers & COMPOSE_MOD_RELEASE) != 0)
        return result_unhandled();

    /*
     * No pending accent: only consume one of the accent keys. Ordinary text
     * and shortcuts stay unhandled so the application/toolkit receives them
     * exactly as usual.
     */
    if (state->pending == PENDING_NONE)
    {
        if (is_modifier_shortcut(modifiers))
            return result_unhandled();

        gunichar literal;
        PendingAccent accent;

        if (pending_from_keyval(keyval, &accent, &literal))
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
    if (keyval == SC_KEY_ESCAPE || keyval == SC_KEY_BACKSPACE)
    {
        compose_state_reset(state);

        return result_handled(NULL);
    }

    if (is_modifier_shortcut(modifiers))
        return result_unhandled();

    gunichar input;

    if (keyval_to_unicode(keyval, &input))
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

    if (is_non_printable_navigation_key(keyval))
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

void compose_result_clear(ComposeResult* result)
{
    if (result == NULL)
        return;

    g_clear_pointer(&result->commit, g_free);
    result->handled = FALSE;
}
