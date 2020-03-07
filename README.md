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

See [`docs/hacking.md`](docs/hacking.md) for this copy, or the [official documentation](https://developer.puri.sm/projects/squeekboard/) for the current release.
