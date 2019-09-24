Hacking
=======

This document describes the standards for modifying and maintaining the *squeekboard* project.

Testing
-------

Most common testing is done in CI. Occasionally, and for each release, do perform manual tests to make sure that

- the application draws correctly
- it shows when relevant
- it changes layouts
- it changes levels

Testing with an application:

```
python3 tests/entry.py
```

Testing visibility:

```
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b true
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b false
```

Testing layouts:

```
$ gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'us'), ('xkb', 'ua')]"
$ gsettings set org.gnome.desktop.input-sources current 1
```

Maintenance
-----------

Squeekboard uses Rust & Cargo for some of its dependencies.

Use the `cargo.sh` script for maintaining the Cargo part of the build. The script takes the usual Cargo commands, after the first 2 positionsl arguments: source directory, and output artifact. So, `cargo test` becomes:

```
cd build_dir
sh /source_path/cargo.sh '' test
```

### Cargo dependencies

Dependencies must be specified in `Cargo.toml` with 2 numbers: "major.minor". Since bugfix version number is meant to not affect the interface, this allows for safe updates.

`Cargo.lock` is used for remembering the revisions of all Rust dependencies. It should be updated often, preferably with each bugfix revision, and in a commit on its own:

```
cd build_dir
sh /source_path/cargo.sh '' update
ninja test
```
