*squeekboard* - a Wayland virtual keyboard
========================================

*Squeekboard* is a virtual keyboard supporting Wayland, built primarily for the *Librem 5* phone.

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

Running
-------

```
$ rootston
# if you used --prefix in your meson command, include the following command
$ export GSETTINGS_SCHEMA_DIR=$YOUR_PREFIX/share/glib-2.0/schemas
$ cd ../build/
$ src/squeekboard
$ busctl call --user org.fedorahosted.Eekboard /org/fedorahosted/Eekboard org.fedorahosted.Eekboard ShowKeyboard
$ busctl call --user org.fedorahosted.Eekboard /org/fedorahosted/Eekboard org.fedorahosted.Eekboard HideKeyboard
```
