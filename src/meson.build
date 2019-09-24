gnome = import('gnome')
dbus_src = gnome.gdbus_codegen(
    'sm.puri.OSK0',
    join_paths(meson.source_root() / 'data' / 'dbus', 'sm.puri.OSK0.xml')
)

config_h = configure_file(
    input: 'config.h.in',
    output: 'config.h',
    configuration: conf_data
)

sources = [
  config_h,
  'imservice.c',
  'server-context-service.c',
  'wayland.c',
  '../eek/eek.c',
  '../eek/eek-element.c',
  '../eek/eek-gtk-keyboard.c',
  '../eek/eek-keyboard.c',
  '../eek/eek-keyboard-drawing.c',
  '../eek/eek-layout.c',
  '../eek/eek-renderer.c',
  '../eek/eek-types.c',
  '../eek/eek-xml-layout.c',
  '../eek/layersurface.c',
  dbus_src,
  enums,
  '../eekboard/key-emitter.c',
  '../eekboard/eekboard-context-service.c',
  '../eekboard/eekboard-context.c',
  '../eekboard/eekboard-service.c',
  #  '../eekboard/eekboard-xklutil.c',
  squeekboard_resources,
  wl_proto_sources,
]

cc = meson.get_compiler('c')


deps = [
#  dependency('glib-2.0', version: '>=2.26.0'),
  dependency('gio-2.0', version: '>=2.26.0'),
  dependency('gtk+-3.0', version: '>=3.0'),
  dependency('libcroco-0.6'),
  dependency('wayland-client', version: '>=1.14'),
  dependency('xkbcommon'),
  cc.find_library('m'),
  cc.find_library('rt'),
  cc.find_library('dl'),
  cc.find_library('pthread'),
#  dependency('libxklavier'), # FIXME remove
]

rslibs = custom_target(
    'rslibs',
    build_by_default: true,
    build_always_stale: true,
    output: ['librs.a'],
    install: false,
    console: true,
    command: [cargo_script, '@OUTPUT@', 'build']
)

test(
    'rstest',
    cargo_script,
    args: ['', 'test']
)

libsqueekboard = static_library('libsqueekboard',
  sources,
  link_with: [rslibs],
  include_directories: [include_directories('..'), include_directories('../eek')],
  dependencies: deps,
  c_args: [
    '-DTHEMESDIR="' + pkgdatadir + '/themes"',
    '-DKEYBOARDSDIR="' + pkgdatadir + '/keyboards"',
    '-DEEKBOARD_COMPILATION=1',
    '-DEEK_COMPILATION=1'],
)

squeekboard = executable('squeekboard',
  'server-main.c',
  wl_proto_sources,
  squeekboard_resources,
  link_with: libsqueekboard,
  include_directories: [include_directories('..'), include_directories('../eek')],
  dependencies: deps,
  install: true,
  c_args: [
    '-DTHEMESDIR="' + pkgdatadir + '/themes"',
    '-DKEYBOARDSDIR="' + pkgdatadir + '/keyboards"',
    '-DEEKBOARD_COMPILATION=1',
    '-DEEK_COMPILATION=1'],
)
