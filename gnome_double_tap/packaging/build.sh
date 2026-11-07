#!/usr/bin/env bash
set -euo pipefail
base=$(cd -- "$(dirname -- "$0")/.." && pwd)
out="$base/dist"
mkdir -p "$out"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
extension="$stage/usr/share/gnome-shell/extensions/double-tap@freeman4848.local"
mkdir -p "$stage/DEBIAN" "$stage/usr/bin" \
  "$stage/usr/lib/double-tap-switcher" "$stage/usr/share/applications" \
  "$stage/usr/lib/systemd/system" "$stage/usr/share/doc/double-tap-switcher" \
  "$stage/usr/share/icons/hicolor/scalable/apps" "$extension/icons"
cat > "$stage/DEBIAN/control" <<'CONTROL'
Package: double-tap-switcher
Version: 1.25
Architecture: all
Maintainer: Freeman4848
Depends: python3, python3-gi, gir1.2-gtk-3.0, gnome-shell (>= 50), gnome-shell (<< 51), systemd, libglib2.0-bin
Section: utils
Priority: optional
Description: Double Tap Switcher for GNOME 50
 GTK settings, panel control and isolated socket-activated double-tap reader.
CONTROL
install -m755 "$base/packaging/double-tap-switcher" "$stage/usr/bin/"
install -m644 "$base/app.py" "$base/settings.py" "$base/evdev_bridge.py" \
  "$base/reader_service.py" "$stage/usr/lib/double-tap-switcher/"
install -m644 "$base/packaging/double-tap-switcher.desktop" "$stage/usr/share/applications/"
install -m644 "$base/packaging/"*.socket "$base/packaging/"*.service "$stage/usr/lib/systemd/system/"
for script in postinst prerm postrm; do
  install -m755 "$base/packaging/$script" "$stage/DEBIAN/$script"
done
install -m644 "$base/metadata.json" "$base/extension.js" "$extension/"
install -m644 "$base/../icons/"dt-*.svg "$extension/icons/"
install -m644 "$base/../icons/dt-app.svg" \
  "$stage/usr/share/icons/hicolor/scalable/apps/double-tap-switcher.svg"
install -m644 "$base/README.md" "$base/../LICENSE" "$stage/usr/share/doc/double-tap-switcher/"
dpkg-deb --root-owner-group --build "$stage" "$out/double-tap-switcher_1.25_all.deb"
cd "$out"
sha256sum double-tap-switcher_1.25_all.deb > SHA256SUMS
