ibus_component_conf = configuration_data()
ibus_component_conf.set('libexecdir', get_option('prefix') / get_option('libexecdir'))
ibus_component_conf.set('version', meson.project_version())

configure_file(
  input: 'org.freedesktop.IBus.SilentCompose.xml.in',
  output: 'org.freedesktop.IBus.SilentCompose.xml',
  configuration: ibus_component_conf,
  install: true,
  install_dir: get_option('datadir') / 'ibus' / 'component',
)
