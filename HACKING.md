Hacking
=======

This document describes the standards for modifying and maintaining the *squeekboard* project.

Sending patches
---------------

By submitting a change to this project, you agree to license it under the [GPL license version 3](./COPYING), or any later version. You also certify that your contribution fulfills the [Developer's Certificate of Origin 1.1](./dco.txt).

Development environment
-----------------------

*Squeekboard* is regularly built and tested on [the develpment environment](https://developer.puri.sm/Librem5/Development_Environment.html).

Recent Fedora releases are likely to be tested as well.

### Dependencies

On a Debian based system run

```sh
sudo apt-get -y install build-essential
sudo apt-get -y build-dep .
```

For an explicit list of dependencies check the `Build-Depends` entry in the
[debian/control][] file.

Testing
-------

Most common testing is done in CI. Occasionally, and for each release, do perform manual tests to make sure that

- the application draws correctly
- it shows when relevant
- it changes layouts
- it changes views

Testing with an application:

```
python3 tools/entry.py
```

Testing visibility:

```
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b true
$ busctl call --user sm.puri.OSK0 /sm/puri/OSK0 sm.puri.OSK0 SetVisible b false
```

Testing layouts:

Layouts can be selected using the GNOME Settings application.

```
# define all available layouts. First one is currently selected.
$ gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'us'), ('xkb', 'de')]"
```

Coding
------

Code submitted should roughly match the style of surrounding code. Things that will *not* be accepted are ones that often lead to errors:

- skipping brackets `{}` after every `if()`, `else`, and similar

Bad example:

```
if (foo)
  bar();
```

Good example:

```
if (foo) {
  bar();
}
```

- mixing tabs and spaces in the same block of code (or config)

Strongly encouraged:

- don't make lines too long. If it's longer than ~80 characters, it's probably unreadable already, and the code needs to be clarified;
- put operators in the beginning of a continuation line

Bad example:

```
foobar = verylongexpression +
    anotherverylongexpression + 
    yetanotherexpression;
```

Good example:

```
foobar = verylongexpression
    + anotherverylongexpression
    + yetanotherexpression;
```

- use `///` for documentation comments in front of definitions and `/*! ... */` for documentation comments in the beginning of modules (see [Rust doc-comments](https://doc.rust-lang.org/reference/comments.html#doc-comments))

If in doubt, check [PEP8](https://github.com/rust-dev-tools/fmt-rfcs/blob/master/guide/guide.md), the [kernel coding style](https://www.kernel.org/doc/html/v4.10/process/coding-style.html), or the [Rust style guide](https://github.com/rust-dev-tools/fmt-rfcs/blob/master/guide/guide.md).

Maintenance
-----------

Squeekboard uses Rust & Cargo for some of its dependencies.

Use the `cargo.sh` script for maintaining the Cargo part of the build. The script takes the usual Cargo commands, after the first 2 positionsl arguments: source directory, and output artifact. So, `cargo test` becomes:

```
cd build_dir
sh /source_path/cargo.sh test
```

### Cargo dependencies

Dependencies must be specified in `Cargo.toml` with 2 numbers: "major.minor". Since bugfix version number is meant to not affect the interface, this allows for safe updates.

`Cargo.lock` is used for remembering the revisions of all Rust dependencies. It should be updated often, preferably with each bugfix revision, and in a commit on its own:

```
cd build_dir
sh /source_path/cargo.sh update
ninja test
```
