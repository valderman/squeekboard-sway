project(
    'squeekboard',
    'c',
    version: '1.0.9',
    license: 'GPLv3',
    meson_version: '>=0.43.0',
    default_options: [ 'warning_level=1', 'buildtype=debugoptimized', 'c_std=gnu11' ],
)

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, meson.project_name())
dbusdir = join_paths(datadir, 'dbus-1/interfaces')

subdir('data')
subdir('eek')
subdir('src')
