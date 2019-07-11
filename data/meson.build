gnome = import('gnome')

squeekboard_resources = gnome.compile_resources(
   'squeekboard-resources',
   'squeekboard.gresources.xml',

   c_name: 'squeekboard',
)

desktop_file = 'sm.puri.Squeekboard.desktop'

i18n.merge_file('desktop',
    input: desktop_file + '.in',
    output: desktop_file,
    po_dir: '../po',
    install: true,
    install_dir: join_paths(datadir, 'applications'),
    type: 'desktop'
)
