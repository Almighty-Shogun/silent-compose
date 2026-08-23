#!/usr/bin/env sh
set -eu

APP_NAME="silent-compose"
MODULE_NAME="libsilent-compose.so"
COMPONENT_NAME="org.freedesktop.IBus.SilentCompose.xml"
ROOT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

IMMODULES_DIR=""
REFRESH_CACHE=1
UNINSTALL=0

usage() {
    cat <<EOF
usage: sudo ./install.sh [options]

Options:
  --immodules-dir DIR    GTK 4 IM module directory, detected when not given
  --print-immodules-dir  Show the detected GTK 4 IM module directory and exit
  --no-refresh-cache     Install files but do not refresh GTK and IBus caches
  --uninstall            Remove installed files
  -h, --help             Show this help

Files are installed under /usr, matching the paths compiled into the IBus
component file. The GTK 4 module is placed in the host's own IM module
directory, which differs between distributions.

Atomic systems such as Fedora Silverblue, Kinoite, Bazzite, and Aurora have a
read-only /usr. Layer the RPM there with rpm-ostree instead.
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

info() {
    printf '%s\n' "$*"
}

require_root() {
    if [ "$(id -u)" -ne 0 ]; then
        die "installer must run as root"
    fi
}

require_mutable_usr() {
    if [ -f /run/ostree-booted ]; then
        info "this system has a read-only /usr managed by ostree"
        info "install the RPM instead: sudo rpm-ostree install $APP_NAME"
        die "generic installer cannot write to /usr on an atomic system"
    fi
}

detect_immodules_dir() {
    if [ -n "$IMMODULES_DIR" ]; then
        printf '%s\n' "$IMMODULES_DIR"
        return 0
    fi

    if command -v pkg-config >/dev/null 2>&1; then
        libdir=$(pkg-config --variable=libdir gtk4 2>/dev/null || true)
        if [ -n "$libdir" ] && [ -d "$libdir" ]; then
            printf '%s\n' "$libdir/gtk-4.0/immodules"
            return 0
        fi
    fi

    for candidate in \
        /usr/lib64/gtk-4.0/immodules \
        /usr/lib/*/gtk-4.0/immodules \
        /usr/lib/gtk-4.0/immodules; do
        if [ -d "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    if command -v ldconfig >/dev/null 2>&1; then
        libgtk=$(ldconfig -p 2>/dev/null | awk '/libgtk-4\.so\.1 /  { print $NF; exit }')
        if [ -n "$libgtk" ] && [ -e "$libgtk" ]; then
            printf '%s\n' "$(dirname "$libgtk")/gtk-4.0/immodules"
            return 0
        fi
    fi

    return 1
}

check_runtime() {
    if ! command -v ibus >/dev/null 2>&1; then
        info "warning: ibus was not found; install it before selecting the input source"
    fi

    if ! ldd "$ROOT_DIR/usr/libexec/$APP_NAME-ibus" >/dev/null 2>&1; then
        return 0
    fi

    missing=$(ldd "$ROOT_DIR/usr/libexec/$APP_NAME-ibus" | awk '/not found/ { print $1 }')
    if [ -n "$missing" ]; then
        info "warning: unresolved shared libraries for the IBus engine:"
        printf '  %s\n' $missing
        info "warning: install the GTK 4, GLib, and IBus runtime packages for your distribution"
    fi
}

install_tree() {
    [ -d "$ROOT_DIR/usr" ] || die "release payload is missing usr/"
    [ -f "$ROOT_DIR/gtk4/$MODULE_NAME" ] || die "release payload is missing gtk4/$MODULE_NAME"

    install -d -m 0755 /usr/libexec
    install -m 0755 "$ROOT_DIR/usr/libexec/$APP_NAME-ibus" "/usr/libexec/$APP_NAME-ibus"

    install -d -m 0755 /usr/share/ibus/component
    install -m 0644 \
        "$ROOT_DIR/usr/share/ibus/component/$COMPONENT_NAME" \
        "/usr/share/ibus/component/$COMPONENT_NAME"

    install -d -m 0755 "$immodules_dir"
    install -m 0755 "$ROOT_DIR/gtk4/$MODULE_NAME" "$immodules_dir/$MODULE_NAME"

    info "installed GTK 4 module: $immodules_dir/$MODULE_NAME"
}

refresh_cache() {
    if command -v gio-querymodules >/dev/null 2>&1; then
        gio-querymodules "$immodules_dir" || true
    elif command -v gio-querymodules-64 >/dev/null 2>&1; then
        gio-querymodules-64 "$immodules_dir" || true
    else
        info "warning: gio-querymodules was not found; the GTK 4 module cache was not refreshed"
    fi

    if command -v ibus >/dev/null 2>&1; then
        ibus write-cache --system >/dev/null 2>&1 || true
    fi
}

uninstall_app() {
    rm -f "/usr/libexec/$APP_NAME-ibus"
    rm -f "/usr/share/ibus/component/$COMPONENT_NAME"
    rm -f "$immodules_dir/$MODULE_NAME"

    info "removed installed $APP_NAME files"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --immodules-dir)
            [ "$#" -ge 2 ] || die "--immodules-dir requires a value"
            IMMODULES_DIR="$2"
            shift 2
            ;;
        --print-immodules-dir)
            detect_immodules_dir || die "could not detect a GTK 4 IM module directory"
            exit 0
            ;;
        --no-refresh-cache)
            REFRESH_CACHE=0
            shift
            ;;
        --uninstall)
            UNINSTALL=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "unknown argument: $1"
            ;;
    esac
done

require_root
require_mutable_usr

immodules_dir=$(detect_immodules_dir) ||
    die "could not detect a GTK 4 IM module directory. Pass --immodules-dir."

if [ "$UNINSTALL" -eq 1 ]; then
    uninstall_app
    if [ "$REFRESH_CACHE" -eq 1 ]; then
        refresh_cache
    fi
    exit 0
fi

check_runtime
install_tree

if [ "$REFRESH_CACHE" -eq 1 ]; then
    refresh_cache
fi

info "installed $APP_NAME"
info "select 'English (US, intl. Silent Compose)' in your input source settings"
info "log out and back in if the input source is not listed yet"
