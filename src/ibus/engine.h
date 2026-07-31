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

G_END_DECLS
