sources = [
  'server-service.c',
  'server-context-service.c',
  'server-main.c',
  '../eekboard/eekboard-service.c',
  '../eekboard/eekboard-context-service.c',
#	$(srcdir)/eekboard-client.c		\
#	$(srcdir)/eekboard-context.c		\
  '../eekboard/eekboard-xklutil.c',
]

deps = [
#  dependency('glib-2.0', version: '>=2.26.0'),
  dependency('gio-2.0', version: '>=2.26.0'),
  dependency('gtk+-3.0', version: '>=3.0'),
  dependency('eek-gtk-0.90'),
  dependency('libxklavier'), # FIXME remove
]

# Replacement for eekboard-server
squeekboard = executable('squeekboard',
  sources,
  include_directories: [include_directories('..')],
  dependencies: deps,
  install: true,
  c_args: ['-DTHEMESDIR="' + pkgdatadir + '/themes"', '-DEEKBOARD_COMPILATION=1'],
)

