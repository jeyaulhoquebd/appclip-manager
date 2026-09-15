const fs = require('fs');
const path = require('path');

const files = [
  { id: 'main_c', name: 'main.c', path: 'main.c', language: 'c', description: 'GtkApplication shell, single-instance handling, --minimized startup, header bar, and settings dialog' },
  { id: 'autostart_h', name: 'autostart.h', path: 'autostart.h', language: 'c', description: 'XDG Autostart module interface (~/.config/autostart/appclip-manager.desktop)' },
  { id: 'autostart_c', name: 'autostart.c', path: 'autostart.c', language: 'c', description: 'XDG Autostart implementation with /proc/self/exe resolution, atomic file creation & deletion' },
  { id: 'settings_h', name: 'settings.h', path: 'settings.h', language: 'c', description: 'AppSettings struct including start_on_boot, max_history_items, and retention days' },
  { id: 'settings_c', name: 'settings.c', path: 'settings.c', language: 'c', description: 'GKeyFile settings manager for ~/.config/appclip-manager/settings.conf' },
  { id: 'clip_model_h', name: 'clip_model.h', path: 'clip_model.h', language: 'c', description: 'ClipEntry struct, ClipType enum, and memory management prototypes' },
  { id: 'clip_model_c', name: 'clip_model.c', path: 'clip_model.c', language: 'c', description: 'ClipEntry allocation, cloning, hashing, and cleanup' },
  { id: 'db_h', name: 'db.h', path: 'db.h', language: 'c', description: 'SQLite3 storage schema, WAL mode, prepared statements, and pruning prototypes' },
  { id: 'db_c', name: 'db.c', path: 'db.c', language: 'c', description: 'SQLite3 schema migration, insert with dedup, query, pin toggle, and auto-pruning' },
  { id: 'clipboard_monitor_h', name: 'clipboard_monitor.h', path: 'clipboard_monitor.h', language: 'c', description: 'Clipboard monitor interface for X11 (GtkClipboard) and Wayland' },
  { id: 'clipboard_monitor_c', name: 'clipboard_monitor.c', path: 'clipboard_monitor.c', language: 'c', description: 'Owner-change listener, text & image extraction, and thumbnail generator' },
  { id: 'ui_cliplist_h', name: 'ui_cliplist.h', path: 'ui_cliplist.h', language: 'c', description: 'Clipboard UI context and panel constructor' },
  { id: 'ui_cliplist_c', name: 'ui_cliplist.c', path: 'ui_cliplist.c', language: 'c', description: 'Clipboard history GtkListBox, search filter, action buttons, and thumbnail rendering' },
  { id: 'update_checker_h', name: 'update_checker.h', path: 'update_checker.h', language: 'c', description: 'UpdateInfo struct, async update detection, and upgrade callback signatures' },
  { id: 'update_checker_c', name: 'update_checker.c', path: 'update_checker.c', language: 'c', description: 'APT, Snap, and Flatpak upgrade detection and execution using fork+execvp' },
  { id: 'ui_updates_h', name: 'ui_updates.h', path: 'ui_updates.h', language: 'c', description: 'Updates UI context and panel constructor' },
  { id: 'ui_updates_c', name: 'ui_updates.c', path: 'ui_updates.c', language: 'c', description: 'Updates tab GtkListBox with package source badges and upgrade progress bars' },
  { id: 'app_model_h', name: 'app_model.h', path: 'app_model.h', language: 'c', description: 'AppInfo struct definition, PackageType enum, and store prototypes' },
  { id: 'app_model_c', name: 'app_model.c', path: 'app_model.c', language: 'c', description: 'AppInfo allocation, memory cleanup, and store query functions' },
  { id: 'app_scanner_h', name: 'app_scanner.h', path: 'app_scanner.h', language: 'c', description: 'Scanner function declarations and asynchronous thread callback signatures' },
  { id: 'app_scanner_c', name: 'app_scanner.c', path: 'app_scanner.c', language: 'c', description: 'dpkg-query, snap list, flatpak list, and desktop INI parser with fork+execvp' },
  { id: 'app_uninstaller_h', name: 'app_uninstaller.h', path: 'app_uninstaller.h', language: 'c', description: 'Input validation prototype and async pkexec uninstaller declarations' },
  { id: 'app_uninstaller_c', name: 'app_uninstaller.c', path: 'app_uninstaller.c', language: 'c', description: 'Sanitized pkexec execution via fork+execvp pipe and async thread worker' },
  { id: 'ui_applist_h', name: 'ui_applist.h', path: 'ui_applist.h', language: 'c', description: 'UiAppContext state definition, row factory, and filter function prototypes' },
  { id: 'ui_applist_c', name: 'ui_applist.c', path: 'ui_applist.c', language: 'c', description: 'GtkListBox row builder, icon fallback resolution, dialogs, and toast' },
  { id: 'makefile', name: 'Makefile', path: 'Makefile', language: 'makefile', description: 'Standard Makefile compiling unified appclip-manager and appclip-clipboard with autostart.c' },
  { id: 'meson_build', name: 'meson.build', path: 'meson.build', language: 'meson', description: 'Modern Meson build system configuration including autostart.c' },
  { id: 'desktop_entry', name: 'appclip-manager.desktop', path: 'appclip-manager.desktop', language: 'ini', description: 'FreeDesktop application launcher and autostart specification entry' }
];

const result = files.map(f => {
  const content = fs.readFileSync(path.join(__dirname, '..', f.path), 'utf8');
  return {
    ...f,
    content
  };
});

const tsContent = `export interface SourceFile {
  id: string;
  name: string;
  path: string;
  language: string;
  description: string;
  content: string;
}

export const sourceFiles: SourceFile[] = ${JSON.stringify(result, null, 2)};
`;

fs.writeFileSync(path.join(__dirname, '../src/sourceCode.ts'), tsContent, 'utf8');
console.log('Successfully generated src/sourceCode.ts');
