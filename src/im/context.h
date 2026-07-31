#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SC_TYPE_IM_CONTEXT (sc_im_context_get_type ())

/*
 * GType for the GTK 4 Silent Compose IM context.
 *
 * The installed GIO module registers this type under the GTK IM module id
 * "silent-compose".
 */
GType sc_im_context_get_type(void);

/* Test-only constructor; production GTK creates contexts through GIO extension metadata. */
GtkIMContext* im_context_new(void);

#ifndef SC_STATIC_TYPE
/* Register the dynamic GType while the GTK IM module is being loaded. */
void im_context_register_type(GTypeModule* module);
#endif

G_END_DECLS
