project(
    'squeekboard',
    'c', 'rust',
    version: '1.0.10',
    license: 'GPLv3',
    meson_version: '>=0.43.0',
    default_options: [ 'warning_level=1', 'buildtype=debugoptimized', 'c_std=gnu11' ],
)

i18n = import('i18n')

if get_option('buildtype').startswith('debug')
    add_project_arguments('-DDEBUG=1', language : 'c')
endif
if get_option('buildtype') != 'plain'
    add_project_arguments('-fstack-protector-strong', language: 'c')
endif

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, meson.project_name())
if get_option('depdatadir') == ''
  depdatadir = datadir
else
  depdatadir = get_option('depdatadir')
endif
dbusdir = join_paths(depdatadir, 'dbus-1/interfaces')

subdir('data')
subdir('protocols')
subdir('eek')
subdir('src')
subdir('po')
