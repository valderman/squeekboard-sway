test_layout = custom_target('squeekboard-test-layout',
    build_by_default: true,
    # meson doesn't track all inputs, cargo does
    build_always_stale: true,
    output: ['squeekboard-test-layout'],
    console: true,
    command: [cargo_build] + cargo_build_flags
        + ['--rename', 'test_layout', '@OUTPUT@', '--bin', 'test_layout'],
    install: true,
    install_dir: bindir,
)
