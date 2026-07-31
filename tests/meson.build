test_silent_compose = executable(
  'test-silent-compose',
  'test-silent-compose.c',
  link_with: test_lib,
  dependencies: [gtk4_dep, gio_dep],
  c_args: common_c_args,
  include_directories: include_directories('../src/im'),
)

test(
  'silent-compose-unit',
  test_silent_compose,
  env: [
    'G_TEST_SRCDIR=' + meson.current_source_dir(),
    'G_TEST_BUILDDIR=' + meson.current_build_dir(),
  ],
)

test_compose = executable(
  'test-compose',
  'test-compose.c',
  link_with: compose_lib,
  dependencies: [gio_dep],
  c_args: common_c_args,
  include_directories: include_directories('../src/compose'),
)

test(
  'silent-compose-compose',
  test_compose,
)

test(
  'silent-compose-ibus-version',
  find_program('sh'),
  args: [
    files('test-version.sh'),
    ibus_exe.full_path(),
    meson.project_version(),
  ],
)
