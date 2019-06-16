*squeekboard* - a Wayland virtual keyboard
========================================

*Squeekboard* is a virtual keyboard supporting Wayland, built primarily for the *Librem 5* phone.

Features
--------

### Present

- GTK3
- Custom xml-defined keyboards
- DBus interface to show and hide

### Temporarily dropped

- A settings interface

### TODO

- Use Wayland virtual keyboard protocol
- Use Wayland text input protocol
- Use Wayland input method protocol
- Become a Wayland layer shell
- Remove eekboard-client
- Use sm.puri.OSK0 DBus interface
- Pick up DBus interface files from /usr/share

Building
--------

### Dependencies

See `.gitlab-ci.yml`.

### Build from git repo

```
$ git clone https://source.puri.sm/Librem5/eekboard.git
$ cd eekboard
$ mkdir ../build
$ meson ../build/
$ cd ../build
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
