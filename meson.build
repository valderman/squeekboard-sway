project(
    'squeekboard',
    'c', 'rust',
    version: '1.0.10',
    license: 'GPLv3',
    meson_version: '>=0.49.0',
    default_options: [
        'warning_level=1',
        'buildtype=debugoptimized',
        'c_std=gnu11'
    ]
)

add_project_arguments(
  [
    '-Werror=implicit-function-declaration',
    '-Werror=implicit-fallthrough=3',
    '-Werror=maybe-uninitialized',
#    '-Werror=missing-field-initializers', // fix eek-unicode-keysym-entries first
  ],
  language: 'c'
)

i18n = import('i18n')

conf_data = configuration_data()

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

summary = [
        '',
        '------------------',
        'squeekboard @0@'.format(meson.project_version()),
        '',
        '------------------',
        ''
]
message('\n'.join(summary))

subdir('data')
subdir('protocols')
subdir('eek')
subdir('src')
subdir('po')
subdir('tests')
