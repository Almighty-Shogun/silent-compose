#include "compose.h"

#include <glib.h>

/**
 * Keysyms the compose tests feed to the state machine.
 *
 * The tests use the same X11 values the toolkits deliver, so a sequence
 * exercised here matches what a real key event produces.
 */
enum
{
    KEY_BACKSPACE = 0xff08,
    KEY_ESCAPE = 0xff1b,
    KEY_LEFT = 0xff51,
    KEY_SHIFT_L = 0xffe1,
    KEY_EURO_SIGN = 0x20ac,
    KEY_DEAD_ACUTE = 0xfe51,
    KEY_DEAD_CIRCUMFLEX = 0xfe52,
    KEY_DEAD_TILDE = 0xfe53,
    KEY_DEAD_DIAERESIS = 0xfe57,
};

/**
 * Assert one two-key compose sequence produces the expected committed text.
 *
 * The first key must be consumed with no commit, which is what proves the
 * accent stayed private until the sequence completed.
 */
static void assert_sequence(const char* label, const guint first, const guint second, const char* expected)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, first, 0);

    g_assert_true(result.handled);
    g_assert_null(result.commit);

    compose_result_clear(&result);

    result = compose_state_process_key(state, second, 0);

    g_assert_true(result.handled);
    g_assert_cmpstr(result.commit, ==, expected);
    g_assert_false(compose_state_is_pending (state));

    if (g_test_verbose())
        g_message("%s -> %s", label, expected);

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify the required Windows-style accent sequences and literal Space output.
 *
 * These are the sequences the project promises to match Windows on, so a
 * change here is a change in the product rather than in the code.
 */
static void required_sequences(void)
{
    assert_sequence("' + e", '\'', 'e', "é");
    assert_sequence("' + E", '\'', 'E', "É");
    assert_sequence("\" + u", '"', 'u', "ü");
    assert_sequence("\" + U", '"', 'U', "Ü");
    assert_sequence("` + a", '`', 'a', "à");
    assert_sequence("^ + o", '^', 'o', "ô");
    assert_sequence("~ + n", '~', 'n', "ñ");
    assert_sequence("' + c", '\'', 'c', "ç");
    assert_sequence("' + Space", '\'', ' ', "'");
    assert_sequence("\" + Space", '"', ' ', "\"");
    assert_sequence("` + Space", '`', ' ', "`");
    assert_sequence("^ + Space", '^', ' ', "^");
    assert_sequence("~ + Space", '~', ' ', "~");
}

/**
 * Verify XKB dead-key keysyms behave like their printable accent keys.
 *
 * Layouts differ in whether they send a dead key or the plain character, and
 * both have to compose identically.
 */
static void dead_key_keysyms(void)
{
    assert_sequence("dead_acute + e", KEY_DEAD_ACUTE, 'e', "é");
    assert_sequence("dead_acute + c", KEY_DEAD_ACUTE, 'c', "ç");
    assert_sequence("dead_diaeresis + u", KEY_DEAD_DIAERESIS, 'u', "ü");
    assert_sequence("dead_circumflex + o", KEY_DEAD_CIRCUMFLEX, 'o', "ô");
    assert_sequence("dead_tilde + n", KEY_DEAD_TILDE, 'n', "ñ");
}

/**
 * Verify repeating an accent commits both literal accent characters.
 *
 * A repeated accent deliberately misses the Space path and falls through the
 * generic fallback, which is what produces two literals rather than one.
 */
static void repeated_accents(void)
{
    assert_sequence("' + '", '\'', '\'', "''");
    assert_sequence("\" + \"", '"', '"', "\"\"");
    assert_sequence("` + `", '`', '`', "``");
    assert_sequence("^ + ^", '^', '^', "^^");
    assert_sequence("~ + ~", '~', '~', "~~");
}

/**
 * Verify Backspace and Escape silently cancel pending accent state.
 *
 * Both keys must also be consumed, so the application neither deletes text nor
 * receives an Escape that belonged to the input method.
 */
static void cancellation(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, '\'', 0);

    g_assert_true(result.handled);
    g_assert_true(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, KEY_BACKSPACE, 0);

    g_assert_true(result.handled);
    g_assert_null(result.commit);
    g_assert_false(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, '"', 0);

    g_assert_true(result.handled);
    g_assert_true(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, KEY_ESCAPE, 0);

    g_assert_true(result.handled);
    g_assert_null(result.commit);
    g_assert_false(compose_state_is_pending (state));

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify unsupported printable completions fall back to literal text.
 *
 * No keystroke may be lost, so an accent no rule covers is committed together
 * with the character that followed it.
 */
static void unsupported_sequence_fallback(void)
{
    assert_sequence("' + t", '\'', 't', "'t");
    assert_sequence("~ + x", '~', 'x', "~x");
}

/**
 * Verify shortcut-modified keys bypass compose handling.
 *
 * Ctrl, Alt, and Super combinations belong to the application; swallowing them
 * would break copy, paste, and window shortcuts.
 */
static void shortcuts_bypass(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, 'c', COMPOSE_MOD_CONTROL);

    g_assert_false(result.handled);
    g_assert_null(result.commit);

    compose_result_clear(&result);

    result = compose_state_process_key(state, 'v', COMPOSE_MOD_ALT);

    g_assert_false(result.handled);
    g_assert_null(result.commit);

    compose_result_clear(&result);

    result = compose_state_process_key(state, 'p', COMPOSE_MOD_CONTROL | COMPOSE_MOD_SHIFT);

    g_assert_false(result.handled);
    g_assert_null(result.commit);

    compose_result_clear(&result);

    result = compose_state_process_key(state, 't', COMPOSE_MOD_SUPER);

    g_assert_false(result.handled);
    g_assert_null(result.commit);

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify shortcuts still bypass compose while an accent is pending.
 *
 * The pending accent has to survive the shortcut as well, so the sequence can
 * still complete afterwards.
 */
static void shortcuts_bypass_while_pending(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, '\'', 0);

    g_assert_true(result.handled);
    g_assert_true(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, 'c', COMPOSE_MOD_CONTROL);

    g_assert_false(result.handled);
    g_assert_null(result.commit);
    g_assert_true(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, 'e', 0);

    g_assert_true(result.handled);
    g_assert_cmpstr(result.commit, ==, "é");
    g_assert_false(compose_state_is_pending (state));

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify standalone modifier keys do not clear pending accent state.
 *
 * Shift arrives before every capital letter, so treating it as input would
 * break each uppercase composition.
 */
static void modifier_key_while_pending_is_ignored(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, '"', 0);

    g_assert_true(result.handled);
    g_assert_true(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, KEY_SHIFT_L, COMPOSE_MOD_SHIFT);

    g_assert_false(result.handled);
    g_assert_null(result.commit);
    g_assert_true(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, 'U', COMPOSE_MOD_SHIFT);

    g_assert_true(result.handled);
    g_assert_cmpstr(result.commit, ==, "Ü");
    g_assert_false(compose_state_is_pending (state));

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify ordinary ASCII and navigation-like keys bypass with no pending accent.
 *
 * With nothing pending the state machine must be invisible, including for
 * release events and navigation keys.
 */
static void plain_ascii_and_non_printable_bypass(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, 'a', 0);

    g_assert_false(result.handled);
    g_assert_null(result.commit);

    compose_result_clear(&result);

    result = compose_state_process_key(state, KEY_SHIFT_L, COMPOSE_MOD_RELEASE);

    g_assert_false(result.handled);
    g_assert_null(result.commit);

    compose_result_clear(&result);

    result = compose_state_process_key(state, KEY_LEFT, 0);

    g_assert_false(result.handled);
    g_assert_null(result.commit);

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify Level-3 printable symbols are left to the IBus passthrough layer.
 *
 * The shared state machine knows nothing about AltGr, so those symbols have to
 * leave it untouched for the backend to handle.
 */
static void level3_printable_bypass(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, KEY_EURO_SIGN, 0);

    g_assert_false(result.handled);
    g_assert_null(result.commit);
    g_assert_false(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, KEY_EURO_SIGN, COMPOSE_MOD_ALT);

    g_assert_false(result.handled);
    g_assert_null(result.commit);
    g_assert_false(compose_state_is_pending (state));

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify release events do not affect pending accent state.
 *
 * Releasing the accent key arrives before the next press, and reacting to it
 * would cancel every sequence immediately.
 */
static void pending_release_event_is_ignored(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, '\'', 0);

    g_assert_true(result.handled);
    g_assert_true(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, '\'', COMPOSE_MOD_RELEASE);

    g_assert_false(result.handled);
    g_assert_null(result.commit);
    g_assert_true(compose_state_is_pending (state));

    compose_result_clear(&result);

    result = compose_state_process_key(state, 'e', 0);

    g_assert_true(result.handled);
    g_assert_cmpstr(result.commit, ==, "é");

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify navigation keys commit the pending literal and still bypass the key.
 *
 * This is the only case where the result is unhandled and still carries commit
 * text, so the caller both inserts the accent and delivers the key.
 */
static void pending_non_printable_commits_accent_and_bypasses_key(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, '\'', 0);

    g_assert_true(result.handled);

    compose_result_clear(&result);

    result = compose_state_process_key(state, KEY_LEFT, 0);

    g_assert_false(result.handled);
    g_assert_cmpstr(result.commit, ==, "'");
    g_assert_false(compose_state_is_pending (state));

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify explicit reset clears pending accent state.
 *
 * Backends take this path on focus-out and disable, so the key that follows
 * must be treated as if nothing had been typed.
 */
static void focus_reset(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, '\'', 0);

    g_assert_true(compose_state_is_pending (state));

    compose_result_clear(&result);

    compose_state_reset(state);

    g_assert_false(compose_state_is_pending (state));

    result = compose_state_process_key(state, 'e', 0);

    g_assert_false(result.handled);
    g_assert_null(result.commit);

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Verify composed Unicode output is valid UTF-8.
 *
 * Commit text goes straight to clients, where invalid encoding is a protocol
 * error rather than a rendering glitch.
 */
static void unicode_output_is_valid_utf8(void)
{
    ComposeState* state = compose_state_new();
    ComposeResult result = compose_state_process_key(state, '~', 0);

    g_assert_true(result.handled);

    compose_result_clear(&result);

    result = compose_state_process_key(state, 'N', 0);

    g_assert_true(result.handled);
    g_assert_cmpstr(result.commit, ==, "Ñ");
    g_assert_true(g_utf8_validate (result.commit, -1, NULL));

    compose_result_clear(&result);
    compose_state_free(state);
}

/**
 * Register compose-state unit tests.
 *
 * The suite needs no display and no session bus, because the state machine has
 * no toolkit dependencies of its own.
 */
int main(int argc, char** argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/silent-compose-compose/required-sequences", required_sequences);
    g_test_add_func("/silent-compose-compose/dead-key-keysyms", dead_key_keysyms);
    g_test_add_func("/silent-compose-compose/repeated-accents", repeated_accents);
    g_test_add_func("/silent-compose-compose/cancellation", cancellation);
    g_test_add_func("/silent-compose-compose/unsupported-fallback", unsupported_sequence_fallback);
    g_test_add_func("/silent-compose-compose/shortcuts-bypass", shortcuts_bypass);
    g_test_add_func("/silent-compose-compose/shortcuts-bypass-while-pending", shortcuts_bypass_while_pending);
    g_test_add_func("/silent-compose-compose/modifier-key-while-pending-ignored", modifier_key_while_pending_is_ignored);
    g_test_add_func("/silent-compose-compose/plain-ascii-non-printable-bypass", plain_ascii_and_non_printable_bypass);
    g_test_add_func("/silent-compose-compose/level3-printable-bypass", level3_printable_bypass);
    g_test_add_func("/silent-compose-compose/pending-release-event-ignored", pending_release_event_is_ignored);
    g_test_add_func("/silent-compose-compose/pending-non-printable-fallback", pending_non_printable_commits_accent_and_bypasses_key);
    g_test_add_func("/silent-compose-compose/focus-reset", focus_reset);
    g_test_add_func("/silent-compose-compose/unicode-valid-utf8", unicode_output_is_valid_utf8);

    return g_test_run();
}
