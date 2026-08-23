#pragma once

#include <glib.h>

G_BEGIN_DECLS

/**
 * Modifier bits shared by every toolkit backend.
 *
 * Backends translate their native event masks into these bits, so the compose
 * state never depends on GTK or IBus constants. Only release events and
 * shortcut modifiers change how a key is treated.
 */
typedef enum
{
    COMPOSE_MOD_SHIFT = 1 << 0,
    COMPOSE_MOD_CONTROL = 1 << 1,
    COMPOSE_MOD_ALT = 1 << 2,
    COMPOSE_MOD_SUPER = 1 << 3,
    COMPOSE_MOD_RELEASE = 1 << 4,
} ComposeModifiers;

/**
 * Composition state shared by the IBus backend and the unit tests.
 *
 * This header is internal to the project and is not installed as a public
 * development API, so the fields stay deliberately plain. "pending" holds one
 * of the implementation's PendingAccent values, kept as a guint so that
 * private enum does not leak into the header.
 */
typedef struct
{
    guint pending;
    gunichar pending_literal;
} ComposeState;

/**
 * Outcome of one processed key event.
 *
 * "handled" tells the caller to stop propagation for this key, and "commit"
 * carries newly allocated UTF-8 text or NULL when the key was consumed only to
 * update or cancel private pending state. The caller must release the result
 * with compose_result_clear().
 */
typedef struct
{
    gboolean handled;
    char* commit;
} ComposeResult;

/**
 * Allocate a new state with no pending accent.
 *
 * Every engine instance and every test owns its own state, so an unfinished
 * accent can never travel between input contexts.
 */
ComposeState* compose_state_new(void);

/**
 * Free a state allocated by compose_state_new().
 *
 * Pending accents are stored as plain values rather than allocations, so
 * discarding the state is enough to discard an unfinished sequence.
 */
void compose_state_free(ComposeState* state);

/**
 * Drop any pending accent without committing text.
 *
 * Backends call this on reset, focus-out, and disable so a half-finished
 * sequence never reappears in another widget or another application.
 */
void compose_state_reset(ComposeState* state);

/**
 * Return TRUE while an accent key is stored privately.
 *
 * Backends check this before running their own passthrough paths, which keeps
 * a pending accent ahead of any direct symbol commit.
 */
gboolean compose_state_is_pending(const ComposeState* state);

/**
 * Process one normalized key event.
 *
 * This function never exposes preedit text. It either returns unhandled,
 * consumes a pending accent silently, or returns completed UTF-8 text to
 * commit.
 */
ComposeResult compose_state_process_key(ComposeState* state, guint key, guint mods);

/**
 * Release result-owned memory and reset the result to an unhandled state.
 *
 * Callers run this on every result, including unhandled ones, so commit text
 * is freed exactly once no matter which branch produced it.
 */
void compose_result_clear(ComposeResult* result);

G_END_DECLS
