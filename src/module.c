#include "context.h"

#include <gio/gio.h>
#include <gmodule.h>
#include <gtk/gtkimmodule.h>

/*
 * Modern, prefixed GIO module load entry point.
 *
 * The shared object is named libsilent-compose.so, so GIO looks for
 * g_io_silent_compose_load/query/unload.  These functions must remain exported;
 * making them static breaks module discovery.
 */
// NOLINTNEXTLINE(misc-use-internal-linkage)
G_MODULE_EXPORT void g_io_silent_compose_load(GIOModule* module)
{
    g_type_module_use(G_TYPE_MODULE(module));
    im_context_register_type(G_TYPE_MODULE(module));
    g_io_extension_point_implement(GTK_IM_MODULE_EXTENSION_POINT_NAME, SC_TYPE_IM_CONTEXT, "silent-compose", 10);
}

/*
 * GIO unload entry point.  The type system owns registered type metadata for
 * the module lifetime, so there is no per-module cleanup here.
 */
// NOLINTNEXTLINE(misc-use-internal-linkage)
G_MODULE_EXPORT void g_io_silent_compose_unload(GIOModule* module) {}

/*
 * Tell GIO which extension point this module implements.  GTK then instantiates
 * SC_TYPE_IM_CONTEXT when gtk-im-module=silent-compose is selected.
 */
// NOLINTNEXTLINE(misc-use-internal-linkage)
G_MODULE_EXPORT char** g_io_silent_compose_query(void)
{
    char* extension_points[] = {(char*)GTK_IM_MODULE_EXTENSION_POINT_NAME, NULL};

    return g_strdupv(extension_points);
}

/* Legacy, unprefixed wrapper retained for loaders that still probe this name. */
G_MODULE_EXPORT void g_io_module_load(GIOModule* module)
{
    g_io_silent_compose_load(module);
}

/* Legacy unload wrapper. */
G_MODULE_EXPORT void g_io_module_unload(GIOModule* module)
{
    g_io_silent_compose_unload(module);
}

/* Legacy query wrapper. */
G_MODULE_EXPORT char** g_io_module_query(void)
{
    return g_io_silent_compose_query();
}
