#include "engine.h"
#include "config.h"

#include <ibus.h>
#include <locale.h>

#define ENGINE_NAME "silent-compose"
#define BUS_NAME "org.freedesktop.IBus.SilentCompose"

/**
 * Session bus connection and engine factory owned by this process.
 *
 * IBus owns this process lifecycle. The component XML launches the executable,
 * then the process registers one factory for ENGINE_NAME on the session bus.
 */
static IBusBus* bus = NULL;
static IBusFactory* factory = NULL;

/**
 * Report whether the argument asks for the version.
 *
 * Keeping --version cheap and side-effect free lets tests and package
 * validation run it without a session bus or a running ibus-daemon.
 */
static gboolean argument_is_version(const char* arg)
{
    return g_strcmp0(arg, "--version") == 0 || g_strcmp0(arg, "-V") == 0;
}

/**
 * Exit the main loop when the session IBus daemon goes away.
 *
 * Quitting instead of reconnecting lets ibus-daemon restart the component the
 * same way it started it.
 */
static void bus_disconnected_cb(IBusBus* unused_bus, void* const data)
{
    ibus_quit();
}

/**
 * Normal startup path for the IBus component executable.
 *
 * The process does not daemonize itself; ibus-daemon starts and supervises it
 * through the installed component XML. Startup only requests the well-known
 * bus name, registers the engine type, and enters ibus_main().
 */
int main(const int argc, char** argv)
{
    if (argc == 2 && argument_is_version(argv[1]))
    {
        g_print("Silent Compose %s\n", SC_VERSION);

        return 0;
    }

    setlocale(LC_ALL, "");

    ibus_init();

    const gboolean debug = g_strcmp0(g_getenv("SILENT_COMPOSE_DEBUG"), "1") == 0;

    bus = ibus_bus_new();

    if (bus == NULL || !ibus_bus_is_connected(bus))
    {
        g_printerr("silent-compose-ibus: ibus-daemon is not available\n");
        g_clear_object(&bus);

        return 1;
    }

    g_signal_connect(bus, "disconnected", G_CALLBACK (bus_disconnected_cb), NULL);

    factory = ibus_factory_new(ibus_bus_get_connection(bus));

    ibus_factory_add_engine(factory, ENGINE_NAME, SC_TYPE_ENGINE);

    const guint32 request_name_reply = ibus_bus_request_name(bus, BUS_NAME, 0);

    if (request_name_reply == 0)
    {
        g_printerr("silent-compose-ibus: failed to request bus name %s\n", BUS_NAME);

        g_clear_object(&factory);
        g_clear_object(&bus);

        return 1;
    }

    if (debug)
        g_debug("silent-compose-ibus started");

    ibus_main();

    g_clear_object(&factory);
    g_clear_object(&bus);

    return 0;
}
