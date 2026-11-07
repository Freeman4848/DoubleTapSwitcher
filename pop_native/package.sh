#!/usr/bin/env bash
set -euo pipefail
base=$(cd -- "$(dirname -- "$0")/.." && pwd)
out="$base/pop_native/dist"; mkdir -p "$out"
stage=$(mktemp -d); trap 'rm -rf -- "$stage"' EXIT
lib="$stage/usr/lib/double-tap-switcher-pop"
mkdir -p "$stage/DEBIAN" "$lib/icons" "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/lib/systemd/system" "$stage/usr/share/doc/double-tap-switcher-pop"
cat > "$stage/DEBIAN/control" <<'CONTROL'
Package: double-tap-switcher-pop
Version: 1.36
Architecture: amd64
Maintainer: Freeman4848
Depends: libc6 (>= 2.38), libstdc++6 (>= 13), libgcc-s1, libsystemd0, libqt6core6t64, libqt6gui6t64, libqt6widgets6t64, libqt6dbus6t64, libqt6network6t64, qt6-qpa-plugins, libqt6waylandclient6, qt6-wayland, libatspi2.0-0, systemd
Conflicts: double-tap-switcher
Section: utils
Priority: optional
Description: Experimental Double Tap Switcher for Pop!_OS KDE X11
 Test build only. GUI password fields are not detected. Not a production release.
CONTROL
install -m755 "$base/pop_native/build/double-tap-switcher-pop" "$base/pop_native/build/double-tap-reader-pop" "$lib/"
install -m644 "$base/pop_native/build/icons/"*.png "$lib/icons/"
cat > "$stage/usr/bin/double-tap-switcher-pop" <<'LAUNCH'
#!/bin/sh
case "${XDG_SESSION_TYPE:-wayland}" in
 x11) export QT_QPA_PLATFORM=xcb ;;
 *) export QT_QPA_PLATFORM=wayland ;;
esac
exec /usr/lib/double-tap-switcher-pop/double-tap-switcher-pop "$@"
LAUNCH
chmod 755 "$stage/usr/bin/double-tap-switcher-pop"
cat > "$stage/usr/share/applications/double-tap-switcher-pop.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Double Tap Switcher (Pop!_OS TEST)
Comment=Experimental KDE/Wayland build; not certified for password fields
Exec=/usr/bin/double-tap-switcher-pop
Icon=/usr/lib/double-tap-switcher-pop/icons/dt-app.png
Terminal=false
Categories=Utility;Accessibility;
DESKTOP
cat > "$stage/usr/lib/systemd/system/double-tap-switcher-pop.socket" <<'SOCKET'
[Unit]
Description=Double Tap Switcher Pop!_OS test socket
[Socket]
ListenStream=/run/double-tap-switcher-pop.sock
SocketMode=0666
Accept=yes
RemoveOnStop=yes
[Install]
WantedBy=sockets.target
SOCKET
cat > "$stage/usr/lib/systemd/system/double-tap-switcher-pop@.service" <<'SERVICE'
[Unit]
Description=Double Tap Switcher Pop!_OS experimental input worker
[Service]
ExecStart=/usr/lib/double-tap-switcher-pop/double-tap-reader-pop
StandardInput=socket
StandardOutput=null
StandardError=null
User=root
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
RestrictAddressFamilies=AF_UNIX
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectControlGroups=yes
CapabilityBoundingSet=CAP_DAC_READ_SEARCH
UMask=0077
SERVICE
cat > "$stage/DEBIAN/preinst" <<'PRE'
#!/bin/sh
set -e
. /etc/os-release
[ "$ID" = pop ] && [ "$VERSION_ID" = 24.04 ] || { echo 'This test build requires Pop!_OS 24.04.' >&2; exit 1; }
PRE
cat > "$stage/DEBIAN/postinst" <<'POST'
#!/bin/sh
set -e
if [ "$1" = configure ]; then
 systemctl daemon-reload
 systemctl enable --now double-tap-switcher-pop.socket
fi
POST
cat > "$stage/DEBIAN/prerm" <<'PRE'
#!/bin/sh
set -e
if [ "$1" = remove ] || [ "$1" = deconfigure ]; then
 systemctl disable --now double-tap-switcher-pop.socket || true
 systemctl stop 'double-tap-switcher-pop@*.service' || true
fi
PRE
chmod 755 "$stage/DEBIAN/"{preinst,postinst,prerm}
install -m644 "$base/LICENSE" "$stage/usr/share/doc/double-tap-switcher-pop/copyright"
install -m644 "$base/pop_native/README.md" "$stage/usr/share/doc/double-tap-switcher-pop/"
dpkg-deb --root-owner-group --build "$stage" "$out/double-tap-switcher-pop_1.36_amd64.deb"
(cd "$out" && sha256sum double-tap-switcher-pop_1.36_amd64.deb > SHA256SUMS)
