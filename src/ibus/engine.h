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
 * Return TRUE and write UTF-8 text to buf when an unhandled IBus key event
 * carries a Windows US-International AltGr symbol that this backend should
 * commit directly. altgr covers sessions that report Right Alt through Mod1
 * while the key is held.
 */
gboolean sc_engine_passthrough_text(guint key, guint code, guint state, gboolean altgr, char* buf, gsize buf_size);

G_END_DECLS
