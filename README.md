# AppClip Manager

A lightweight native Linux desktop utility for Ubuntu, built in **C** with **GTK3** and **SQLite3**, that combines two everyday tools into one app:

1. **Installed Apps Manager** — view and uninstall applications installed via APT, Snap, or Flatpak.
2. **Clipboard History Manager** — automatically tracks everything you copy (text and images) so you never lose a clipboard item again.

![Platform](https://img.shields.io/badge/platform-Ubuntu%2022.04%2B-orange)
![Language](https://img.shields.io/badge/language-C-blue)
![GUI](https://img.shields.io/badge/GUI-GTK3-green)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

---

## ✨ Features

### 📦 Installed Apps Manager
- Lists all installed applications from **APT**, **Snap**, and **Flatpak**, merged into a single searchable view.
- Shows app icon, name, package source, and installed size.
- Live search/filter as you type.
- One-click **Uninstall** with a confirmation dialog and safe privilege escalation via `pkexec`.
- Background scanning so the UI never freezes.

### 📋 Clipboard History
- Monitors the system clipboard in real time — works with `Ctrl+C`, right-click copy, or any app.
- Supports both **text** and **image** clips.
- Compatible with both **X11** (native GTK clipboard) and **Wayland** (via `wl-clipboard` fallback).
- Persistent history stored locally in **SQLite3**.
- **Copy Again**, **Save As...**, **Pin**, and **Delete** actions on every clip.
- Configurable auto-cleanup by item count or age.
- Optional system tray icon for quick access to recent clips.

---

## 🖥️ Screenshots

*(Add screenshots here once available — e.g. `docs/screenshot-apps.png`, `docs/screenshot-clipboard.png`)*

---

## 📥 Installation

### Option 1: Install via `.deb` package (recommended)

```bash
sudo dpkg -i appclip-manager_1.0.0_amd64.deb
sudo apt --fix-broken install   # if any dependency is missing
```

### Option 2: Build from source

**Dependencies:**

```bash
sudo apt update
sudo apt install build-essential pkg-config libgtk-3-dev libsqlite3-dev
sudo apt install libappindicator3-dev   # optional, for system tray icon
sudo apt install wl-clipboard           # optional, for Wayland clipboard support
```

**Build:**

```bash
git clone <your-repo-url>
cd appclip-manager
make
```

**Run:**

```bash
chmod +x appclip-manager
./appclip-manager
```

**Install system-wide:**

```bash
chmod +x install.sh
./install.sh
```

This copies the binary to `/usr/local/bin/`, the `.desktop` launcher to `/usr/share/applications/`, and the app icons to `/usr/share/icons/hicolor/`.

---

## 🗑️ Uninstallation

If installed via `.deb`:
```bash
sudo dpkg -r appclip-manager
```

If installed via `install.sh`:
```bash
sudo rm /usr/local/bin/appclip-manager
sudo rm /usr/share/applications/appclip-manager.desktop
sudo rm /usr/share/icons/hicolor/*/apps/appclip-manager.png
sudo rm /usr/share/icons/hicolor/scalable/apps/appclip-manager.svg
sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor
```

---

## 🏗️ Project Structure

```
appclip-manager/
├── main.c                 # Application entry point, window & tab setup
├── app_scanner.c/.h        # Scans APT / Snap / Flatpak / .desktop entries
├── app_uninstaller.c/.h    # Handles uninstall commands via pkexec
├── app_model.c/.h          # AppInfo data structure
├── ui_applist.c/.h         # Installed apps list UI
├── clipboard_monitor.c/.h  # X11/Wayland clipboard change detection
├── db.c/.h                 # SQLite3 wrapper for clipboard history
├── clip_model.c/.h         # ClipEntry data structure
├── ui_cliplist.c/.h        # Clipboard history UI
├── settings.c/.h           # App settings (max items, retention days)
├── assets/                 # Logo and icons (hicolor theme structure)
├── Makefile                # Build configuration
├── install.sh               # System-wide installation script
└── appclip-manager.desktop # Desktop launcher entry
```

---

## ⚙️ Configuration

Settings are stored in:
```
~/.config/appclip-manager/settings.conf
```

| Setting             | Default | Description                              |
|----------------------|---------|-------------------------------------------|
| `max_history_items`  | 500     | Maximum number of clipboard entries kept  |
| `max_history_days`   | 30      | Auto-delete clips older than this (days)  |

Clipboard data is stored in:
```
~/.local/share/appclip-manager/history.db      # SQLite database
~/.local/share/appclip-manager/clips/           # Saved image clips (PNG)
```

---

## 🔧 Notes on Wayland Support

GTK's native clipboard signal (`owner-change`) is unreliable on Wayland sessions. For full clipboard monitoring on Wayland, install:

```bash
sudo apt install wl-clipboard
```

AppClip Manager automatically detects the session type (`$XDG_SESSION_TYPE`) and falls back to `wl-paste --watch` when running under Wayland.

---

## 🛡️ Security Notes

- All external commands (`dpkg`, `snap`, `flatpak`, `pkexec`) are executed using `fork()` + `execvp()` with argument arrays — **never** via `system()` — to prevent shell/command injection.
- All SQLite queries use prepared statements with bound parameters to prevent SQL injection.
- Uninstall actions always require explicit user confirmation and, where needed, root authorization via `pkexec`.

---

## 🤝 Contributing

Contributions, bug reports, and feature requests are welcome. Please open an issue or submit a pull request.

---

## 📄 License

This project is licensed under the MIT License — see the `LICENSE` file for details.

---

## 👤 Author

**Jeyaul Hoque**
🌐 [https://jeyaulhoque.pages.dev/](https://jeyaulhoque.pages.dev/)

---

## 🙏 Acknowledgements

Built with:
- [GTK3](https://www.gtk.org/) — GUI toolkit
- [SQLite3](https://www.sqlite.org/) — embedded database
- [wl-clipboard](https://github.com/bugaevc/wl-clipboard) — Wayland clipboard utilities