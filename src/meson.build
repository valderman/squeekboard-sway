sources = [
  'server-service.c',
  'server-context-service.c',
  'server-main.c',
]

deps = [
#  dependency('glib-2.0', version: '>=2.26.0'),
  dependency('gio-2.0', version: '>=2.26.0'),
  dependency('gtk+-3.0', version: '>=3.0'),
  dependency('eekboard-0.90'),
  dependency('eek-gtk-0.90'),
]

# Replacement for eekboard-server
squeekboard = executable('squeekboard',
  sources,
  dependencies: deps,
  install: true,
  c_args: '-DTHEMESDIR="' + pkgdatadir + '/themes"',
)

