squeekboard-sway
================

*Squeekboard* is a virtual keyboard supporting Wayland, built primarily for the *Librem 5* phone.

It squeaks because some Rust got inside.

*Squeekboard-Sway* is a small set of experimental modifications to Squeekboard.
The modifications are intended to facilitate using the [Sway](https://swaywm.org) window manager
with an on-screen keyboard.

It adds the following features compared to upstream:

* The height of the on-screen keyboard can be forced by setting the `SQUEEKBOARD_HEIGHT`
    environment variable to the desired keyboard height (in pixels)
* Shift and Mod4 (AKA super/logo/meta/windows key) are now valid modifier keys
* Modifier keys are cleared after performing a modifier key combo (such as Mod4+Shift+q)
* Swedish keyboard layout with Shift and Mod4 modifiers


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

Running
-------

```
$ phoc # if no compatible Wayland compositor is running yet
$ cd ../build/
$ src/squeekboard
```

Developing
----------

See [`doc/hacking.md`](doc/hacking.md) for this copy, or the [official documentation](https://developer.puri.sm/projects/squeekboard/) for the current release.
