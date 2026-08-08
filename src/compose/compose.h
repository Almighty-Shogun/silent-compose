#pragma once

#include <glib.h>

G_BEGIN_DECLS

/*
 * Modifier bits used by the toolkit backends after translating their native
 * key-event masks.  The compose state only needs to know whether a key is a
 * release event or part of a shortcut that must not be consumed.
 */
typedef enum
{
    COMPOSE_MOD_SHIFT = 1 << 0,
    COMPOSE_MOD_CONTROL = 1 << 1,
    COMPOSE_MOD_ALT = 1 << 2,
    COMPOSE_MOD_SUPER = 1 << 3,
    COMPOSE_MOD_RELEASE = 1 << 4,
} ComposeModifiers;

/*
 * Small composition state shared by the IBus backend and unit tests.
 *
 * The fields are intentionally simple because this header is internal to the
 * project and is not installed as a public development API.  "pending" stores
 * one of the implementation's PendingAccent values, kept as guint here so the
 * private enum does not leak into the header.
 */
typedef struct
{
    guint pending;
    gunichar pending_literal;
} ComposeState;

/*
 * Result returned for one processed key event.
 *
 * handled:
 *   TRUE when the caller must stop propagation for this key.
 * commit:
 *   Newly allocated UTF-8 text to commit, or NULL when the key was consumed
 *   only to update/cancel private pending state.  The caller must clear the
 *   result with compose_result_clear().
 */
typedef struct
{
    gboolean handled;
    char* commit;
} ComposeResult;

/* Allocate a new state with no pending accent. */
ComposeState* compose_state_new(void);

/* Free a state allocated by compose_state_new(). */
void compose_state_free(ComposeState* state);

/* Drop any pending accent without committing text. */
void compose_state_reset(ComposeState* state);

/* Return TRUE while an accent key is stored privately. */
gboolean compose_state_is_pending(const ComposeState* state);

/*
 * Process one normalized key event.
 *
 * This function never exposes preedit text.  It either returns unhandled,
 * consumes a pending accent silently, or returns completed UTF-8 text to commit.
 */
ComposeResult compose_state_process_key(ComposeState* state, guint key, guint mods);

/* Release result-owned memory and reset the result to an unhandled state. */
void compose_result_clear(ComposeResult* result);

G_END_DECLS
