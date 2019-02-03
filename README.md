eekboard - an easy to use virtual keyboard toolkit
==================================================

eekboard is a virtual keyboard software package, including a set of
tools to implement desktop virtual keyboards.

*squeekboard* is the Wayland support branch.

Building
--------

### Dependencies

REQUIRED: GLib2, GTK-3.0, PangoCairo, libxklavier, libcroco
OPTIONAL: libXtst, at-spi2-core, IBus, Clutter, Clutter-Gtk, Python, Vala, gobject-introspection, libcanberra

### Build from git repo

```
$ git clone git://github.com/ueno/eekboard.git
$ cd eekboard
$ ./autogen.sh --prefix=/usr --enable-gtk-doc
$ make
$ sudo make install
```

### Build from tarball

```
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

Running
-------

```
$ rootston
$ export GSETTINGS_SCHEMA_DIR=$YOUR_PREFIX/share/glib-2.0/schemas
$ eekboard-server &
$ eekboard
```
