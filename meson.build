# Meson build file for AppClip Manager (Parts 1 & 2)
# Author: Jeyaul Hoque (https://jeyaulhoque.pages.dev/)

project('appclip-manager', 'c',
  version: '1.0.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
    'buildtype=release'
  ]
)

# Required GTK3, GLib, and SQLite3 dependencies
gtk_dep = dependency('gtk+-3.0', version: '>= 3.24.0')
glib_dep = dependency('glib-2.0', version: '>= 2.50.0')
sqlite_dep = dependency('sqlite3', version: '>= 3.20.0')

m_dep = meson.get_compiler('c').find_library('m', required: false)

common_deps = [gtk_dep, glib_dep, sqlite_dep, m_dep]

clip_sources = files(
  'db.c',
  'clip_model.c',
  'clipboard_monitor.c',
  'ui_cliplist.c',
  'settings.c'
)

app_sources = files(
  'app_model.c',
  'app_scanner.c',
  'app_uninstaller.c',
  'ui_applist.c'
)

# 1. Primary Unified App
executable('appclip-manager',
  files('main.c') + app_sources + clip_sources,
  dependencies: common_deps,
  install: true
)

# 2. Standalone Clipboard History App
executable('appclip-clipboard',
  files('clipboard_main.c') + clip_sources,
  dependencies: common_deps,
  install: true
)

# Install assets (128x128 logo)
install_data('assets/logo.png',
  install_dir: get_option('datadir') / 'appclip-manager'
)
