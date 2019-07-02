
install_data(
    'themes/default.css',
    install_dir: pkgdatadir + '/themes',
)

install_data(
    'keyboards/keyboards.xml',
    install_dir: pkgdatadir + '/keyboards/',
)

install_data(
    'keyboards/geometry/compact.xml',
    install_dir: pkgdatadir + '/keyboards/geometry/',
)

symbols = [
    'ar.xml',
    'as-inscript.xml',
    'be.xml',
    'bn-inscript.xml',
    'fa.xml',
    'gu-inscript.xml',
    'he.xml',
    'hi-inscript.xml',
    'ja-kana.xml',
    'kk.xml',
    'kn-inscript.xml',
    'ks-inscript.xml',
    'ks.xml',
    'mai-inscript.xml',
    'ml-inscript.xml',
    'mr-inscript.xml',
    'my.xml',
    'or-inscript.xml',
    'pa-inscript.xml',
    'ru.xml',
    'sd-inscript.xml',
    'ta-inscript.xml',
    'te-inscript.xml',
    'th.xml',
    'ua.xml',
    'ug.xml',
    'us.xml',
    'zh-bopomofo.xml',
]

foreach symbol: symbols
    install_data(
        'keyboards/symbols/' + symbol,
        install_dir: pkgdatadir + '/keyboards/symbols/',
    )
endforeach

desktop_file = 'sm.puri.Squeekboard.desktop'

i18n.merge_file('desktop',
    input: desktop_file + '.in',
    output: desktop_file,
    po_dir: '../po',
    install: true,
    install_dir: join_paths(datadir, 'applications'),
    type: 'desktop'
)
