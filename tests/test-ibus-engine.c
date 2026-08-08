#include "engine.h"

#include <glib.h>

enum
{
    KEY_A = 'a',
    KEY_EURO_SIGN = 0x20ac,
};

static void level3_printable_commit_text(void)
{
    char buffer[7] = {0};

    g_assert_true(sc_engine_make_passthrough_text(KEY_EURO_SIGN, IBUS_MOD5_MASK, buffer, sizeof buffer));
    g_assert_cmpstr(buffer, ==, "€");
}

static void level3_printable_allows_altgr_mod1_combo(void)
{
    char buffer[7] = {0};

    g_assert_true(sc_engine_make_passthrough_text(KEY_EURO_SIGN,
                                                 IBUS_MOD5_MASK | IBUS_MOD1_MASK,
                                                 buffer,
                                                 sizeof buffer));
    g_assert_cmpstr(buffer, ==, "€");
}

static void plain_ascii_stays_unhandled(void)
{
    char buffer[7] = {0};

    g_assert_false(sc_engine_make_passthrough_text(KEY_A, 0, buffer, sizeof buffer));
    g_assert_cmpstr(buffer, ==, "");
}

static void shortcut_modified_printable_stays_unhandled(void)
{
    char buffer[7] = {0};

    g_assert_false(sc_engine_make_passthrough_text(KEY_EURO_SIGN, IBUS_CONTROL_MASK, buffer, sizeof buffer));
    g_assert_cmpstr(buffer, ==, "");
}

static void left_alt_printable_stays_unhandled(void)
{
    char buffer[7] = {0};

    g_assert_false(sc_engine_make_passthrough_text(KEY_EURO_SIGN, IBUS_MOD1_MASK, buffer, sizeof buffer));
    g_assert_cmpstr(buffer, ==, "");
}

static void release_event_stays_unhandled(void)
{
    char buffer[7] = {0};

    g_assert_false(sc_engine_make_passthrough_text(KEY_EURO_SIGN,
                                                  IBUS_MOD5_MASK | IBUS_RELEASE_MASK,
                                                  buffer,
                                                  sizeof buffer));
    g_assert_cmpstr(buffer, ==, "");
}

int main(int argc, char** argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/silent-compose-ibus-engine/level3-printable-commit-text", level3_printable_commit_text);
    g_test_add_func("/silent-compose-ibus-engine/level3-printable-allows-altgr-mod1-combo",
                    level3_printable_allows_altgr_mod1_combo);
    g_test_add_func("/silent-compose-ibus-engine/plain-ascii-unhandled", plain_ascii_stays_unhandled);
    g_test_add_func("/silent-compose-ibus-engine/shortcut-modified-printable-unhandled",
                    shortcut_modified_printable_stays_unhandled);
    g_test_add_func("/silent-compose-ibus-engine/left-alt-printable-unhandled", left_alt_printable_stays_unhandled);
    g_test_add_func("/silent-compose-ibus-engine/release-event-unhandled", release_event_stays_unhandled);

    return g_test_run();
}
