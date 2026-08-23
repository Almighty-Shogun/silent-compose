#include "engine.h"

#include <glib.h>

/**
 * Keysyms the passthrough tests feed to the engine.
 *
 * These are the values IBus delivers, including the already translated symbols
 * that some layouts produce before the engine sees the event.
 */
enum
{
    KEY_A = 'a',
    KEY_APOSTROPHE = '\'',
    KEY_5 = '5',
    KEY_6 = '6',
    KEY_R = 'r',
    KEY_E_DIAERESIS = 0x00eb,
    KEY_EURO_SIGN = 0x20ac,
    KEY_OE = 0x0153,
};

/**
 * Evdev keycodes for the keys under test.
 *
 * IBus reports evdev codes, which are 8 lower than the XKB keycodes shown by
 * tools such as wev, so the table is written in evdev terms.
 */
enum
{
    CODE_5 = 6,
    CODE_6 = 7,
    CODE_F = 33,
    CODE_R = 19,
    CODE_APOSTROPHE = 40,
    CODE_X = 45,
};

/**
 * Assert an IBus key event produces direct passthrough text.
 *
 * Testing the mapping function directly keeps the whole AltGr table verifiable
 * without a session bus or a running ibus-daemon.
 */
static void expect_passthrough(const guint key, const guint code, const guint state, const gboolean altgr, const char* expected)
{
    char buf[7] = {0};

    g_assert_true(sc_engine_passthrough_text(key, code, state, altgr, buf, sizeof buf));

    g_assert_cmpstr(buf, ==, expected);
}

/**
 * Assert an IBus key event is left unhandled by passthrough mapping.
 *
 * The buffer must stay empty as well, so a rejected event can never leave
 * stale text behind for a later commit.
 */
static void expect_unhandled(const guint key, const guint code, const guint state, const gboolean altgr)
{
    char buf[7] = {0};

    g_assert_false(sc_engine_passthrough_text(key, code, state, altgr, buf, sizeof buf));

    g_assert_cmpstr(buf, ==, "");
}

/**
 * Verify already translated Level-3 Euro keysyms are committed.
 *
 * Layouts that resolve AltGr themselves hand the engine the final symbol, and
 * it still has to reach the client.
 */
static void translated_level3(void)
{
    expect_passthrough(KEY_EURO_SIGN, CODE_5, IBUS_MOD5_MASK, FALSE, "€");
}

/**
 * Verify translated Level-3 Euro survives layouts that also report Mod1.
 *
 * Several sessions set Mod1 alongside Mod5 for Right Alt, which must not make
 * the event look like a Left Alt shortcut.
 */
static void translated_level3_with_mod1(void)
{
    expect_passthrough(KEY_EURO_SIGN, CODE_5, IBUS_MOD5_MASK | IBUS_MOD1_MASK, FALSE, "€");
}

/**
 * Verify translated Level-3 symbols work when no physical keycode is present.
 *
 * Synthetic and remote events arrive with keycode zero, so the lookup has to
 * fall back to matching on the keysym.
 */
static void translated_level3_without_code(void)
{
    expect_passthrough(KEY_EURO_SIGN, 0, IBUS_MOD5_MASK, FALSE, "€");
}

/**
 * Verify raw AltGr+5 maps to Euro through the Windows table.
 *
 * This is the path taken when the layout leaves the translation to the input
 * method, which is how a plain US layout behaves.
 */
static void raw_mod5_euro(void)
{
    expect_passthrough(KEY_5, CODE_5, IBUS_MOD5_MASK, FALSE, "€");
}

/**
 * Verify engine-local Right Alt tracking maps Mod1 AltGr+5 to Euro.
 *
 * Sessions that never set Mod5 are the reason the engine tracks the modifier
 * itself, and this is the case that proves the tracking works.
 */
static void active_altgr_mod1_euro(void)
{
    expect_passthrough(KEY_5, CODE_5, IBUS_MOD1_MASK, TRUE, "€");
}

/**
 * Verify Super/Mod4 combinations are never treated as AltGr text.
 *
 * Those chords belong to the desktop shell, so committing a character for them
 * would break window management shortcuts.
 */
static void active_altgr_mod4_blocked(void)
{
    expect_unhandled(KEY_5, CODE_5, IBUS_MOD4_MASK, TRUE);
}

/**
 * Verify the evdev keycode table does not map RightAlt+F by accident.
 *
 * Keys absent from the Windows table have to stay unhandled even while an
 * AltGr session is active.
 */
static void active_altgr_f_unhandled(void)
{
    expect_unhandled('f', CODE_F, IBUS_MOD1_MASK, TRUE);
}

/**
 * Verify raw AltGr+6 maps to the Windows fraction character.
 *
 * The number row differs between the Windows and Linux international layouts,
 * so it is checked explicitly.
 */
static void raw_mod5_fraction(void)
{
    expect_passthrough(KEY_6, CODE_6, IBUS_MOD5_MASK, FALSE, "¼");
}

/**
 * Verify AltGr+R follows Windows US-International rather than Linux altgr-intl.
 *
 * Matching Windows is the point of this table, and R is where the two layouts
 * visibly disagree.
 */
static void right_alt_r_matches_windows(void)
{
    expect_passthrough(KEY_R, CODE_R, IBUS_MOD5_MASK, FALSE, "®");
}

/**
 * Verify Linux-translated AltGr+R is corrected back to Windows behavior.
 *
 * When the layout has already produced its own symbol, the lookup recognizes
 * it by keycode and replaces it with the Windows one.
 */
static void translated_linux_altgr_intl_r_is_corrected(void)
{
    expect_passthrough(KEY_E_DIAERESIS, CODE_R, IBUS_MOD5_MASK, FALSE, "®");
}

/**
 * Verify unmapped shifted AltGr entries remain unhandled.
 *
 * An entry with no shifted symbol on Windows must produce nothing rather than
 * falling back to its unshifted character.
 */
static void right_alt_shift_r_unhandled(void)
{
    expect_unhandled(KEY_R, CODE_R, IBUS_MOD5_MASK | IBUS_SHIFT_MASK, FALSE);
}

/**
 * Verify symbols outside the Windows table are not committed.
 *
 * The Linux layout offers characters Windows does not, and those must be left
 * to the application instead of being adopted.
 */
static void linux_altgr_intl_extra_blocked(void)
{
    expect_unhandled(KEY_OE, CODE_X, IBUS_MOD5_MASK, FALSE);
}

/**
 * Verify base dead-key characters stay in compose handling.
 *
 * An unmodified apostrophe belongs to ComposeState, so passthrough claiming it
 * would break every acute sequence.
 */
static void base_dead_key_unhandled(void)
{
    expect_unhandled(KEY_APOSTROPHE, CODE_APOSTROPHE, 0, FALSE);
}

/**
 * Verify AltGr accent keys produce the Windows spacing accent characters.
 *
 * With AltGr held the same key yields a standalone accent instead of starting
 * a composition.
 */
static void altgr_apostrophe_literals(void)
{
    expect_passthrough(KEY_APOSTROPHE, CODE_APOSTROPHE, IBUS_MOD5_MASK, FALSE, "´");
    expect_passthrough(KEY_APOSTROPHE, CODE_APOSTROPHE, IBUS_MOD5_MASK | IBUS_SHIFT_MASK, FALSE, "¨");
}

/**
 * Verify ordinary ASCII is not consumed by passthrough mapping.
 *
 * Committing plain letters here would duplicate the text the application
 * already receives from the key event.
 */
static void plain_ascii_unhandled(void)
{
    expect_unhandled(KEY_A, 0, 0, FALSE);
}

/**
 * Verify Ctrl-modified events are left to applications.
 *
 * Shortcuts are refused before any table lookup, whatever symbol the layout
 * attached to the key.
 */
static void shortcut_unhandled(void)
{
    expect_unhandled(KEY_EURO_SIGN, CODE_5, IBUS_CONTROL_MASK, FALSE);
}

/**
 * Verify plain Left Alt is not treated as AltGr.
 *
 * Only a tracked Right Alt session or a real Mod5 bit may enable passthrough,
 * or menu accelerators would stop working.
 */
static void left_alt_unhandled(void)
{
    expect_unhandled(KEY_EURO_SIGN, CODE_5, IBUS_MOD1_MASK, FALSE);
}

/**
 * Verify key release events are ignored.
 *
 * Handling both edges of a key would commit every AltGr symbol twice.
 */
static void release_unhandled(void)
{
    expect_unhandled(KEY_EURO_SIGN, CODE_5, IBUS_MOD5_MASK | IBUS_RELEASE_MASK, FALSE);
}

/**
 * Register IBus passthrough mapping tests.
 *
 * The suite exercises the exported mapping function only, so it runs without a
 * session bus or a running ibus-daemon.
 */
int main(int argc, char** argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/silent-compose-ibus-engine/translated-level3", translated_level3);
    g_test_add_func("/silent-compose-ibus-engine/translated-level3-with-mod1", translated_level3_with_mod1);
    g_test_add_func("/silent-compose-ibus-engine/translated-level3-without-code", translated_level3_without_code);
    g_test_add_func("/silent-compose-ibus-engine/raw-mod5-euro", raw_mod5_euro);
    g_test_add_func("/silent-compose-ibus-engine/active-altgr-mod1-euro", active_altgr_mod1_euro);
    g_test_add_func("/silent-compose-ibus-engine/active-altgr-mod4-blocked", active_altgr_mod4_blocked);
    g_test_add_func("/silent-compose-ibus-engine/active-altgr-f-unhandled", active_altgr_f_unhandled);
    g_test_add_func("/silent-compose-ibus-engine/raw-mod5-fraction", raw_mod5_fraction);
    g_test_add_func("/silent-compose-ibus-engine/right-alt-r-matches-windows", right_alt_r_matches_windows);
    g_test_add_func("/silent-compose-ibus-engine/translated-linux-altgr-intl-r-is-corrected", translated_linux_altgr_intl_r_is_corrected);
    g_test_add_func("/silent-compose-ibus-engine/right-alt-shift-r-unhandled", right_alt_shift_r_unhandled);
    g_test_add_func("/silent-compose-ibus-engine/linux-altgr-intl-extra-blocked", linux_altgr_intl_extra_blocked);
    g_test_add_func("/silent-compose-ibus-engine/base-dead-key-unhandled", base_dead_key_unhandled);
    g_test_add_func("/silent-compose-ibus-engine/altgr-apostrophe-literals", altgr_apostrophe_literals);
    g_test_add_func("/silent-compose-ibus-engine/plain-ascii-unhandled", plain_ascii_unhandled);
    g_test_add_func("/silent-compose-ibus-engine/shortcut-unhandled", shortcut_unhandled);
    g_test_add_func("/silent-compose-ibus-engine/left-alt-unhandled", left_alt_unhandled);
    g_test_add_func("/silent-compose-ibus-engine/release-unhandled", release_unhandled);

    return g_test_run();
}
