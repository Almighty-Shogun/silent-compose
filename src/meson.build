im_sources = files(
  'im/context.c',
)

im_headers = files(
  'im/context.h',
)

compose_sources = files(
  'compose/compose.c',
)

compose_headers = files(
  'compose/compose.h',
)

module_sources = files(
  'module.c',
)

ibus_sources = files(
  'ibus/engine.c',
  'ibus/main.c',
)

ibus_headers = files(
  'ibus/engine.h',
)

version_conf = configuration_data()
version_conf.set_quoted('SC_VERSION', meson.project_version())

version_header = configure_file(
  output: 'config.h',
  configuration: version_conf,
)

compose_lib = static_library(
  'silent-compose-compose',
  compose_sources,
  dependencies: [gio_dep],
  c_args: common_c_args,
  include_directories: include_directories('compose'),
  extra_files: compose_headers,
)

gtk_module = shared_module(
  'silent-compose',
  im_sources + module_sources,
  dependencies: [gtk4_dep, gio_dep, gmodule_dep],
  c_args: common_c_args,
  include_directories: include_directories('im'),
  extra_files: im_headers,
  gnu_symbol_visibility: 'hidden',
  install: true,
  install_dir: gtk4_immodules_dir,
)

test_lib = static_library(
  'silent-compose-test',
  im_sources,
  dependencies: [gtk4_dep, gio_dep],
  c_args: common_c_args + ['-DSC_STATIC_TYPE'],
  include_directories: include_directories('im'),
  extra_files: im_headers,
)

ibus_exe = executable(
  'silent-compose-ibus',
  ibus_sources + version_header,
  link_with: compose_lib,
  dependencies: [ibus_dep, gio_dep],
  c_args: common_c_args,
  include_directories: include_directories('compose', 'ibus'),
  extra_files: ibus_headers,
  install: true,
  install_dir: get_option('libexecdir'),
)
