gnome = import('gnome')

enum_headers = [
  'eek-symbol.h',
  'eek-types.h',
]

enums = gnome.mkenums_simple('eek-enumtypes', sources: enum_headers)

marshalers = gnome.genmarshal(
  'eek-marshalers',
  sources: ['eek-marshalers.list'],
  prefix: '_eek_marshal',
  internal: true,
)

python = find_program('python3')

gen_keysym_entries_special = generator(
  python,
  arguments: ['@CURRENT_SOURCE_DIR@/gen-keysym-entries.py', 'special_keysym_entries', '@INPUT@'],
  capture: true,
  output: 'eek-@BASENAME@.h',
)

gen_keysym_entries_unicode = generator(
  python,
  arguments: ['@CURRENT_SOURCE_DIR@/gen-keysym-entries.py', 'unicode_keysym_entries', '@INPUT@'],
  capture: true,
  output: 'eek-@BASENAME@.h',
)

gen_keysym_entries_xkeysym = generator(
  python,
  arguments: ['@CURRENT_SOURCE_DIR@/gen-keysym-entries.py', 'xkeysym_keysym_entries', '@INPUT@'],
  capture: true,
  output: 'eek-@BASENAME@.h',
)

keysym_entries = [
  gen_keysym_entries_special.process('./special-keysym-entries.txt'),
  gen_keysym_entries_unicode.process('./unicode-keysym-entries.txt'),
  gen_keysym_entries_xkeysym.process('./xkeysym-keysym-entries.txt'),
]
