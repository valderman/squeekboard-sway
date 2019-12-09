#!/usr/bin/env python3

import gi
import sys
gi.require_version('Gtk', '3.0')

from gi.repository import Gtk

try:
    terminal = [("Terminal", Gtk.InputPurpose.TERMINAL)]
except AttributeError:
    print("Terminal purpose not available on this GTK version", file=sys.stderr)
    terminal = []

class App(Gtk.Application):

    purposes = [
        ("Free form", Gtk.InputPurpose.FREE_FORM),
        ("Alphabetical", Gtk.InputPurpose.ALPHA),
        ("Digits", Gtk.InputPurpose.DIGITS),
        ("Number", Gtk.InputPurpose.NUMBER),
        ("Phone", Gtk.InputPurpose.PHONE),
        ("URL", Gtk.InputPurpose.URL),
        ("E-mail", Gtk.InputPurpose.EMAIL),
        ("Name", Gtk.InputPurpose.NAME),
        ("Password", Gtk.InputPurpose.PASSWORD),
        ("PIN", Gtk.InputPurpose.PIN),
    ] + terminal

    def do_activate(self):
        w = Gtk.ApplicationWindow(application=self)
        grid = Gtk.Grid(orientation='vertical', column_spacing=8, row_spacing=8)
        i = 0
        for text, purpose in self.purposes:

            l = Gtk.Label(label=text)
            e = Gtk.Entry(hexpand=True)
            e.set_input_purpose(purpose)
            grid.attach(l, 0, i, 1, 1)
            grid.attach(e, 1, i, 1, 1)
            i += 1

        w.add(grid)
        w.show_all()

app = App()
app.run(sys.argv)
