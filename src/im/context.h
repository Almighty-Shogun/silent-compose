#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SC_TYPE_IM_CONTEXT (sc_im_context_get_type ())

/**
 * GType for the GTK 4 Silent Compose IM context.
 *
 * The installed GIO module registers this type under the GTK IM module id
 * "silent-compose".
 */
GType sc_im_context_get_type(void);

/**
 * Create a context directly for the unit tests.
 *
 * Production GTK never calls this; it instantiates the type through the GIO
 * extension point metadata the module publishes.
 */
GtkIMContext* im_context_new(void);

#ifndef SC_STATIC_TYPE
/**
 * Register the dynamic GType while the GTK IM module is being loaded.
 *
 * G_DEFINE_DYNAMIC_TYPE keeps its registration function static, so the module
 * entry point in module.c reaches it through this wrapper.
 */
void im_context_register_type(GTypeModule* module);
#endif

G_END_DECLS
