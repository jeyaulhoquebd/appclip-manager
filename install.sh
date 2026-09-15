#!/usr/bin/env bash
# install.sh - Installer script for AppClip Manager
# Created by: Jeyaul Hoque
# Website: https://jeyaulhoque.pages.dev/

set -e

echo "=== Building AppClip Manager ==="
make -j$(nproc 2>/dev/null || echo 2)

echo "=== Installing AppClip Manager ==="

# Check for root / sudo permissions
if [ "$(id -u)" -ne 0 ]; then
    SUDO="sudo"
    echo "Elevating permissions with sudo..."
else
    SUDO=""
fi

# 1. Install binary to /usr/local/bin
echo "Installing binary to /usr/local/bin/appclip-manager..."
$SUDO install -d /usr/local/bin
$SUDO install -m 755 appclip-manager /usr/local/bin/appclip-manager
if [ -f appclip-clipboard ]; then
    $SUDO install -m 755 appclip-clipboard /usr/local/bin/appclip-clipboard
fi

# 2. Install .desktop file to /usr/share/applications/
echo "Installing desktop shortcut to /usr/share/applications/..."
$SUDO install -d /usr/share/applications
$SUDO install -m 644 appclip-manager.desktop /usr/share/applications/appclip-manager.desktop

# 3. Install icon to /usr/share/icons/hicolor/
echo "Installing application icons in all standard resolutions..."
$SUDO install -d /usr/share/icons/hicolor/scalable/apps
if [ -f assets/icons/appclip-manager.svg ]; then
    $SUDO install -m 644 assets/icons/appclip-manager.svg /usr/share/icons/hicolor/scalable/apps/appclip-manager.svg
fi

for sz in 16 32 48 64 128 256 512; do
    $SUDO install -d /usr/share/icons/hicolor/${sz}x${sz}/apps
    if [ -f assets/icons/appclip-manager-${sz}x${sz}.png ]; then
        $SUDO install -m 644 assets/icons/appclip-manager-${sz}x${sz}.png /usr/share/icons/hicolor/${sz}x${sz}/apps/appclip-manager.png
    elif [ -f assets/logo.png ]; then
        $SUDO install -m 644 assets/logo.png /usr/share/icons/hicolor/${sz}x${sz}/apps/appclip-manager.png
    fi
done

# Also install to application data dir for fallback resolution
$SUDO install -d /usr/share/appclip-manager
if [ -f assets/icons/appclip-manager-128x128.png ]; then
    $SUDO install -m 644 assets/icons/appclip-manager-128x128.png /usr/share/appclip-manager/logo.png
elif [ -f assets/logo.png ]; then
    $SUDO install -m 644 assets/logo.png /usr/share/appclip-manager/logo.png
fi

# 4. Update desktop and icon databases if tools are available
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    echo "Updating GTK icon cache..."
    $SUDO gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    echo "Updating desktop database..."
    $SUDO update-desktop-database /usr/share/applications 2>/dev/null || true
fi

echo ""
echo "=== Installation Completed Successfully! ==="
echo "You can now run 'appclip-manager' from your terminal or application launcher."
