*squeekboard* - a Wayland virtual keyboard
========================================

*Squeekboard* is a virtual keyboard supporting Wayland, built primarily for the *Librem 5* phone.

It squeaks because some Rust got inside.

Features
--------

### Present

- GTK3
- Custom yaml-defined keyboards
- DBus interface to show and hide
- Use Wayland input method protocol to show and hide
- Use Wayland virtual keyboard protocol

### Temporarily dropped

- A settings interface

### TODO

- Pick up user-defined layouts
- Use Wayland input method protocol
- Pick up DBus interface files from /usr/share

Building
--------

### Dependencies

See `.gitlab-ci.yml`.

### Build from git repo

```
$ git clone https://source.puri.sm/Librem5/squeekboard.git
$ cd squeekboard
$ mkdir ../build
$ meson ../build/
$ cd ../build
$ ninja test
$ ninja install
```

For development, alter the `meson` call:

```
$ meson ../build/ --prefix=../install
```

and don't skip `ninja install` before running. The last step is necessary in order to find the keyboard definition files.

Running
-------

```
$ rootston
$ cd ../build/
$ src/squeekboard
```

### Maintenance

Use the `cargo.sh` script for maintaining the Cargo part of the build. The script takes the usual Cargo commands, after the first 2 positionsl arguments: source directory, and output artifact. So, `cargo test` becomes:

```
cd build_dir
/source_path/cargo.sh /source_path '' test
```

### Testing

```
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b true
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b false
$ gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'us'), ('xkb', 'ua')]"
$ gsettings set org.gnome.desktop.input-sources current 1
```
