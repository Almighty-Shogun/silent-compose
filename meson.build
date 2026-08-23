project(
    'silent-compose',
    'c',
    version : run_command(
        'sh',
        '-c',
        '''
if [ -n "${SC_VERSION:-}" ]; then
  printf '%s\n' "${SC_VERSION}" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || exit 1
  printf '%s\n' "${SC_VERSION}"
  exit 0
fi

tag="$(git describe --tags --exact-match 2>/dev/null || true)"

if printf '%s\n' "${tag}" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  printf '%s\n' "${tag}"
  exit 0
fi

for path in \
  "${MESON_SOURCE_ROOT:-}" \
  "$(pwd)" \
  "$(dirname "$(pwd)")" \
  "$(dirname "$(pwd)")"/dist-unpack/silent-compose-*; do
  [ -n "${path}" ] || continue

  directory="$(basename "${path}")"
  version="${directory#silent-compose-}"

  if [ "${directory}" != "${version}" ] &&
     printf '%s\n' "${version}" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    printf '%s\n' "${version}"
    exit 0
  fi
done

printf '0.0.0\n'
        ''',
        check : true,
    ).stdout().strip(),
    license : 'MIT',
    meson_version : '>= 1.2.0',
    default_options : [
        'c_std=c11',
        'prefix=/usr',
        'warning_level=3',
        'werror=false',
    ],
)

gtk4_dep = dependency('gtk4', version : '>= 4.2')
gio_dep = dependency('gio-2.0')
gmodule_dep = dependency('gmodule-2.0')
ibus_dep = dependency('ibus-1.0', version : '>= 1.5.0')

cc = meson.get_compiler('c')

gtk_libdir = gtk4_dep.get_variable(pkgconfig : 'libdir')
gtk_binary_version = gtk4_dep.get_variable(pkgconfig : 'gtk_binary_version', default_value : '4.0.0')
gtk_host = gtk4_dep.get_variable(pkgconfig : 'gtk_host', default_value : host_machine.cpu_family() + '-' + host_machine.system())

configured_immodules_dir = get_option('gtk4_immodules_dir')
if configured_immodules_dir == ''
    gtk4_immodules_dir = gtk_libdir / 'gtk-4.0' / 'immodules'
else
    gtk4_immodules_dir = configured_immodules_dir
endif

message('GTK 4 binary version: ' + gtk_binary_version)
message('GTK 4 host: ' + gtk_host)
message('GTK 4 IM module install dir: ' + gtk4_immodules_dir)

common_c_args = [
    '-DG_LOG_USE_STRUCTURED=1',
    '-DG_LOG_DOMAIN="silent-compose"',
    '-Wno-unused-parameter',
    '-Wno-deprecated-declarations',
    '-Wno-variadic-macros',
] + cc.get_supported_arguments([
                                   '-Wno-error=pedantic',
                               ])

subdir('src')
subdir('data')
subdir('tests')

gio_querymodules_bits = '64'
if host_machine.cpu_family() == 'x86'
    gio_querymodules_bits = '32'
endif

gio_querymodules = find_program(
    'gio-querymodules-' + gio_querymodules_bits,
    required : false,
)

if gio_querymodules.found()
    meson.add_install_script(
        gio_querymodules,
        gtk4_immodules_dir,
        skip_if_destdir : true,
    )
else
    warning('gio-querymodules-' + gio_querymodules_bits + ' not found; direct installs will not regenerate the GTK 4 IM module cache.')
endif
