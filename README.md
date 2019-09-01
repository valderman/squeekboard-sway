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

### Testing

```
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b true
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b false
$ gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'us'), ('xkb', 'ua')]"
$ gsettings set org.gnome.desktop.input-sources current 1
```
