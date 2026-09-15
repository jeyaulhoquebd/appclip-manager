const fs = require('fs');
const path = require('path');

const files = [
  { id: 'main_c', name: 'main.c', path: 'main.c', language: 'c', description: 'GtkApplication entry point, window layout, header bar, and about dialog' },
  { id: 'app_model_h', name: 'app_model.h', path: 'app_model.h', language: 'c', description: 'AppInfo struct definition, PackageType enum, and store prototypes' },
  { id: 'app_model_c', name: 'app_model.c', path: 'app_model.c', language: 'c', description: 'AppInfo allocation, memory cleanup, and store query functions' },
  { id: 'app_scanner_h', name: 'app_scanner.h', path: 'app_scanner.h', language: 'c', description: 'Scanner function declarations and asynchronous thread callback signatures' },
  { id: 'app_scanner_c', name: 'app_scanner.c', path: 'app_scanner.c', language: 'c', description: 'dpkg-query, snap list, flatpak list, and desktop INI parser with fork+execvp' },
  { id: 'app_uninstaller_h', name: 'app_uninstaller.h', path: 'app_uninstaller.h', language: 'c', description: 'Input validation prototype and async pkexec uninstaller declarations' },
  { id: 'app_uninstaller_c', name: 'app_uninstaller.c', path: 'app_uninstaller.c', language: 'c', description: 'Sanitized pkexec execution via fork+execvp pipe and async thread worker' },
  { id: 'ui_applist_h', name: 'ui_applist.h', path: 'ui_applist.h', language: 'c', description: 'UiAppContext state definition, row factory, and filter function prototypes' },
  { id: 'ui_applist_c', name: 'ui_applist.c', path: 'ui_applist.c', language: 'c', description: 'GtkListBox row builder, icon fallback resolution, dialogs, and toast' },
  { id: 'makefile', name: 'Makefile', path: 'Makefile', language: 'makefile', description: 'Standard Makefile using pkg-config --cflags/--libs gtk+-3.0' },
  { id: 'meson_build', name: 'meson.build', path: 'meson.build', language: 'meson', description: 'Alternative modern Meson build system configuration' }
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
