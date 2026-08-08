#pragma once

#include <ibus.h>

G_BEGIN_DECLS

#define SC_TYPE_ENGINE (sc_engine_get_type ())

/*
 * GType for the Silent Compose IBus engine.
 *
 * The component XML names this engine "silent-compose"; ibus-daemon creates
 * instances through the factory registered in main.c.
 */
GType sc_engine_get_type(void);

/*
 * Internal helper exposed for tests.
 *
 * Return TRUE and write UTF-8 text to buffer when an unhandled IBus key event
 * carries printable non-ASCII text that this backend should commit directly.
 */
gboolean sc_engine_make_passthrough_text(guint keyval, guint state, char* buffer, gsize buffer_size);

G_END_DECLS
