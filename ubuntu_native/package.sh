#!/usr/bin/env bash
set -euo pipefail
base=$(cd -- "$(dirname -- "$0")/.." && pwd)
version=1.33
out="$base/ubuntu_native/dist"
stage=$(mktemp -d)
trap 'rm -r -- "$stage"' EXIT
lib="$stage/usr/lib/double-tap-switcher"
extension="$stage/usr/share/gnome-shell/extensions/double-tap@freeman4848.local"
mkdir -p "$out" "$stage/DEBIAN" "$stage/usr/bin" "$lib/icons" \
 "$stage/usr/lib/systemd/system" "$stage/usr/share/applications" \
 "$stage/usr/share/icons/hicolor/256x256/apps" "$extension/icons" \
 "$stage/usr/share/doc/double-tap-switcher" "$stage/etc/xdg/autostart"
cat > "$stage/DEBIAN/control" <<EOF
Package: double-tap-switcher
Version: $version
Architecture: amd64
Maintainer: Freeman4848
Depends: libc6 (>= 2.38), libstdc++6 (>= 15), libgcc-s1, libsystemd0, libqt6core6t64, libqt6gui6, libqt6widgets6, libqt6dbus6, libqt6network6, qt6-qpa-plugins, gnome-shell (>= 50), gnome-shell (<< 51), systemd, libglib2.0-bin
Section: utils
Priority: optional
Description: Double Tap Switcher C++ client and reader for Ubuntu GNOME 50
 C++/Qt frontend and isolated reader with the GNOME JavaScript bridge.
EOF
install -m755 "$base/ubuntu_native/build/double-tap-switcher" \
 "$base/ubuntu_native/build/double-tap-reader" "$lib/"
install -m644 "$base/ubuntu_native/build/icons/"*.png "$lib/icons/"
cat > "$stage/usr/bin/double-tap-switcher" <<'EOF'
#!/bin/sh
export QT_QPA_PLATFORM=xcb
exec /usr/lib/double-tap-switcher/double-tap-switcher "$@"
EOF
chmod 755 "$stage/usr/bin/double-tap-switcher"
install -m644 "$base/gnome_double_tap/extension.js" "$extension/"
printf '{"uuid":"double-tap@freeman4848.local","name":"Double Tap Switcher","description":"C++ client and GNOME bridge","shell-version":["50"],"version":12,"version-name":"%s"}\n' "$version" > "$extension/metadata.json"
install -m644 "$base/icons/"dt-*.svg "$extension/icons/"
install -m644 "$base/ubuntu_native/build/icons/dt-app.png" \
 "$stage/usr/share/icons/hicolor/256x256/apps/double-tap-switcher.png"
src="$base/gnome_double_tap/packaging"
install -m644 "$src/double-tap-switcher.desktop" "$stage/usr/share/applications/"
cat > "$stage/etc/xdg/autostart/double-tap-switcher.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Double Tap Switcher
Exec=/usr/bin/double-tap-switcher --background
TryExec=/usr/bin/double-tap-switcher
Icon=double-tap-switcher
OnlyShowIn=GNOME;
X-GNOME-Autostart-enabled=true
X-GNOME-Autostart-Delay=3
Terminal=false
EOF
install -m644 "$src/double-tap-switcher.socket" "$stage/usr/lib/systemd/system/"
sed 's@ExecStart=.*@ExecStart=/usr/lib/double-tap-switcher/double-tap-reader@' \
 "$src/double-tap-switcher@.service" > "$stage/usr/lib/systemd/system/double-tap-switcher@.service"
for script in postinst prerm postrm; do
  install -m755 "$src/$script" "$stage/DEBIAN/$script"
done
sed -i "/systemctl daemon-reload/i\    systemctl stop 'double-tap-switcher@*.service' || true" "$stage/DEBIAN/postinst"
install -m644 "$base/LICENSE" "$base/ubuntu_native/TEST_REPORT.md" \
 "$stage/usr/share/doc/double-tap-switcher/"
dpkg-deb --root-owner-group --build "$stage" "$out/double-tap-switcher_${version}_amd64.deb"
(cd "$base" && sha256sum "ubuntu_native/dist/double-tap-switcher_${version}_amd64.deb" > ubuntu_native/dist/SHA256SUMS)
