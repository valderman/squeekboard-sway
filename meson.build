project(
    'squeekboard',
    'c', 'rust',
    version: '1.6.0',
    license: 'GPLv3',
    meson_version: '>=0.51.0',
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
    '-Werror=missing-field-initializers',
    '-Werror=incompatible-pointer-types',
    '-Werror=int-conversion',
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
if get_option('buildtype') == 'release'
    cargo_build_flags = ['--release'] # for artifacts
else
    cargo_build_flags = []
endif

prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
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

dep_cargo = find_program('cargo')
cargo_script = find_program('cargo.sh')
cargo_build = find_program('cargo_build.sh')

subdir('data')
subdir('protocols')
subdir('src')
subdir('tools')
subdir('tests')
