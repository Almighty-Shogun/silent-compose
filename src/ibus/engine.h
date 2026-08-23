#pragma once

#include <ibus.h>

G_BEGIN_DECLS

#define SC_TYPE_ENGINE (sc_engine_get_type ())

/**
 * GType for the Silent Compose IBus engine.
 *
 * The component XML names this engine "silent-compose"; ibus-daemon creates
 * instances through the factory registered in main.c.
 */
GType sc_engine_get_type(void);

/**
 * Map an unhandled IBus key event to Windows US-International AltGr text.
 *
 * Returns TRUE and writes UTF-8 text to buf when the event carries a symbol
 * this backend should commit directly. altgr covers sessions that report Right
 * Alt through Mod1 while the key is held, where no Mod5 bit ever arrives.
 * The symbol is exposed so the mapping can be tested without an ibus-daemon.
 */
gboolean sc_engine_passthrough_text(guint key, guint code, guint state, gboolean altgr, char* buf, gsize buf_size);

G_END_DECLS
