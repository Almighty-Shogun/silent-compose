<a href="https://shogun.ms" target="_blank" rel="noopener">
	<img src="https://cdn.shogun.ms/assets/branding/app-icon-256.svg" alt="Shogun app-icon" height="62"/>
</a>

---

# Silent Compose

Silent Compose is a Fedora GNOME Wayland input method for silent
US-International-style composition.

```text
Press '        -> nothing visible
Then press e   -> é
```

The pending accent stays private. Applications only receive the final committed
text.

## Supported

- Fedora Workstation 44
- GNOME Wayland
- IBus
- GTK 4
- x86_64

Verified in Brave native Wayland, Electron apps, JetBrains IDEs, and GTK 4 apps.

## Features

- GNOME input source: `English (US, intl. Silent Compose)`
- IBus backend for system-wide Wayland use
- GTK 4 IM module
- No visible preedit text
- No app-specific flags or launcher edits
- No X11 wrapper
- Shortcuts such as Ctrl+C, Ctrl+V, Alt, and Super still pass through

## Examples

| First key | Second key | Output |
| --- | --- | --- |
| `'` | `e` | `é` |
| `'` | `E` | `É` |
| `"` | `u` | `ü` |
| `"` | `U` | `Ü` |
| `` ` `` | `a` | `à` |
| `^` | `o` | `ô` |
| `~` | `n` | `ñ` |
| `'` | `c` | `ç` |
| `'` | `C` | `Ç` |
| `'` | `Space` | `'` |
| `"` | `Space` | `"` |
| `` ` `` | `Space` | `` ` `` |
| `^` | `Space` | `^` |
| `~` | `Space` | `~` |
| `'` | `'` | `''` |
| `"` | `"` | `""` |

etc...

Unsupported printable sequences preserve input. Example: `'` then `t` commits
`'t`.

## Install / uninstall

Download and install a release RPM:

```bash
curl -LO "https://github.com/Almighty-Shogun/silent-compose/releases/latest/download/silent-compose.fc44.rpm"
sudo dnf install ./silent-compose.fc44.rpm
```

Restart the GNOME IBus user service so GNOME can see the newly installed engine:

```bash
systemctl --user restart org.freedesktop.IBus.session.GNOME.service
```

Then select Silent Compose in GNOME Settings:

```text
Settings -> Keyboard -> Input Sources -> English (US, intl. Silent Compose)
```

Use `English (US, intl. Silent Compose)`, not GNOME's built-in
`English (US, intl., with dead keys)`.

Check:

```bash
gsettings get org.gnome.desktop.input-sources sources
ibus engine
```

Expected:

```text
[('ibus', 'silent-compose')]
silent-compose
```

If the engine still does not appear, log out and back in.


Uninstall:

```bash
sudo dnf remove silent-compose
```

## Build

Install build dependencies:

```bash
sudo dnf install gcc meson ninja-build pkgconf-pkg-config gtk4-devel glib2-devel ibus-devel
```

Build:

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Release versions come from plain Git tags such as `0.2.0`. Untagged local builds
use the development fallback version `0.0.0`.

Fedora RPMs are produced by the GitHub release workflow.

## Files installed

Typical Fedora paths:

```text
/usr/lib64/gtk-4.0/immodules/libsilent-compose.so
/usr/libexec/silent-compose-ibus
/usr/share/ibus/component/org.freedesktop.IBus.SilentCompose.xml
```

The package does not modify GNOME settings, keyboard layouts, or application
launchers.

## Debugging

```bash
SILENT_COMPOSE_DEBUG=1
```

Keep debug logging disabled during normal typing.
