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

## 💻 Supported

- Fedora Workstation 44, packaged as an RPM
- Fedora Atomic variants, layered with `rpm-ostree`
- Debian 12 and Ubuntu 22.04 or newer, packaged as a `.deb`
- Other x86_64 distributions, through the generic `.run` installer
- GNOME Wayland
- IBus 1.5 or newer
- GTK 4.2 or newer
- x86_64

Verified in Brave native Wayland, Electron apps, JetBrains IDEs, and GTK 4 apps.

## ✨ Features

- GNOME input source: `English (US, intl. Silent Compose)`
- IBus backend for system-wide Wayland use
- GTK 4 IM module
- No visible preedit text
- No app-specific flags or launcher edits
- No X11 wrapper
- Shortcuts such as Ctrl+C, Ctrl+V, Alt, and Super still pass through

### Examples

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

## 🚀 Installation

### Fedora Workstation, COPR repository
```bash
sudo dnf install dnf-plugins-core
sudo dnf copr enable almighty-shogun/silent-compose
sudo dnf install silent-compose

systemctl --user restart org.freedesktop.IBus.session.GNOME.service
```

### Fedora Atomic, COPR repository
Silverblue, Kinoite, Bazzite, Aurora, and other ostree-based variants layer the
same RPM. They do not all ship `dnf`, so add the COPR repository by writing its
repo file directly:

```bash
sudo curl -o /etc/yum.repos.d/almighty-shogun-silent-compose.repo \
    "https://copr.fedorainfracloud.org/coprs/almighty-shogun/silent-compose/repo/fedora-$(rpm -E %fedora)/almighty-shogun-silent-compose-fedora-$(rpm -E %fedora).repo"
sudo rpm-ostree install silent-compose
systemctl reboot
```

Updates arrive with your normal system upgrade: `sudo dnf upgrade` on Fedora Workstation, or `sudo rpm-ostree upgrade` followed by a reboot on Fedora Atomic.
On Workstation, restart IBus or log out afterwards so the new engine loads.

### Debian and Ubuntu

Download and install the release package:

```bash
curl -LO "https://github.com/Almighty-Shogun/silent-compose/releases/latest/download/silent-compose.deb"
sudo apt install ./silent-compose.deb
```

`apt` pulls in `ibus` and the GTK 4 runtime if they are missing. The package
refreshes the GTK 4 module cache and the IBus registry on install.

### Other distributions

The `.run` installer carries prebuilt x86_64 binaries and works on any
distribution with glibc 2.36 or newer, GTK 4.2 or newer, and IBus 1.5 or newer.
It detects the GTK 4 IM module directory for the host, which differs between
distributions, and installs under `/usr`.

> [!NOTE]
> The `.run` installer refuses to run on atomic systems, because `/usr` is
read-only there. Layer the RPM with `rpm-ostree` instead.

```bash
curl -LO "https://github.com/Almighty-Shogun/silent-compose/releases/latest/download/silent-compose.run"
chmod +x silent-compose.run
sudo ./silent-compose.run
```

Show where the GTK 4 module would be installed, without installing anything:

```bash
./silent-compose.run --print-immodules-dir
```

Pass `--immodules-dir DIR` when detection picks the wrong directory.

### Selecting the input source

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

## 🧹 Uninstall

```bash
# Fedora
sudo dnf remove silent-compose

# Fedora Atomic
sudo rpm-ostree uninstall silent-compose
systemctl reboot

# Debian/Ubuntu
sudo apt remove silent-compose

# Generic
sudo ./silent-compose.run --uninstall
```

## 🔧 Build

Install build dependencies on Fedora:

```bash
sudo dnf install gcc meson ninja-build pkgconf-pkg-config gtk4-devel glib2-devel ibus-devel
```

Install build dependencies on Debian and Ubuntu:

```bash
sudo apt install gcc meson ninja-build pkg-config libgtk-4-dev libglib2.0-dev libibus-1.0-dev
```

Meson 1.2.0 or newer is required. Debian 12 ships 1.0.1, so install a newer
Meson with `pip3 install --break-system-packages "meson>=1.2.0"` there.

Build:

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Release versions come from plain Git tags such as `0.2.0`. Untagged local builds
use the development fallback version `0.0.0`.

Fedora RPMs, Debian packages, and the generic `.run` installer are produced by
the GitHub release workflow. Packaging inputs live under `packaging/fedora`,
`packaging/debian`, and `packaging/run`.

### Files installed

Typical Fedora paths:

```text
/usr/lib64/gtk-4.0/immodules/libsilent-compose.so
/usr/libexec/silent-compose-ibus
/usr/share/ibus/component/org.freedesktop.IBus.SilentCompose.xml
```

Typical Debian and Ubuntu paths:

```text
/usr/lib/x86_64-linux-gnu/gtk-4.0/immodules/libsilent-compose.so
/usr/libexec/silent-compose-ibus
/usr/share/ibus/component/org.freedesktop.IBus.SilentCompose.xml
```

Only the GTK 4 module directory differs between distributions. It is taken from
`gtk4.pc`, so it follows whatever layout the host uses.

The package does not modify GNOME settings, keyboard layouts, or application
launchers.

## 🩺 Debugging

```bash
SILENT_COMPOSE_DEBUG=1
```

The IBus engine also appends key-routing diagnostics to
`/tmp/silent-compose-ibus.log` by default because IBus can redirect engine
stderr to `/dev/null`. Set `SILENT_COMPOSE_DEBUG_LOG` to choose a different
path.

Keep debug logging disabled during normal typing because it records key events.
