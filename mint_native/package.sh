#!/usr/bin/env bash
set -euo pipefail
base=$(cd -- "$(dirname -- "$0")/.." && pwd)
out="$base/mint_native/dist"; mkdir -p "$out"
stage=$(mktemp -d); trap 'rm -rf -- "$stage"' EXIT
lib="$stage/usr/lib/double-tap-switcher-mint"
mkdir -p "$stage/DEBIAN" "$lib/icons" "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/lib/systemd/system" "$stage/usr/share/doc/double-tap-switcher-mint"
cat > "$stage/DEBIAN/control" <<'CONTROL'
Package: double-tap-switcher-mint
Version: 1.38
Architecture: amd64
Maintainer: Freeman4848
Depends: libc6 (>= 2.38), libstdc++6 (>= 13), libgcc-s1, libsystemd0, libx11-6, libxtst6, libqt6core6t64, libqt6gui6t64, libqt6widgets6t64, libqt6dbus6t64, libqt6network6t64, qt6-qpa-plugins, libatspi2.0-0, systemd
Conflicts: double-tap-switcher
Section: utils
Priority: optional
Description: Experimental Double Tap Switcher for Mint Xfce X11
 Test build only. GUI password fields are not detected. Not a production release.
CONTROL
install -m755 "$base/mint_native/build/double-tap-switcher-mint" "$base/mint_native/build/double-tap-reader-mint" "$lib/"
install -m644 "$base/mint_native/build/icons/"*.png "$lib/icons/"
cat > "$stage/usr/bin/double-tap-switcher-mint" <<'LAUNCH'
#!/bin/sh
export QT_QPA_PLATFORM=xcb
exec /usr/lib/double-tap-switcher-mint/double-tap-switcher-mint "$@"
LAUNCH
chmod 755 "$stage/usr/bin/double-tap-switcher-mint"
cat > "$stage/usr/share/applications/double-tap-switcher-mint.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Double Tap Switcher (Mint TEST)
Comment=Experimental Xfce/X11 build; not certified for password fields
Exec=/usr/bin/double-tap-switcher-mint
Icon=/usr/lib/double-tap-switcher-mint/icons/dt-app.png
Terminal=false
Categories=Utility;Accessibility;
DESKTOP
cat > "$stage/usr/lib/systemd/system/double-tap-switcher-mint.socket" <<'SOCKET'
[Unit]
Description=Double Tap Switcher Mint test socket
[Socket]
ListenStream=/run/double-tap-switcher-mint.sock
SocketMode=0666
Accept=yes
RemoveOnStop=yes
[Install]
WantedBy=sockets.target
SOCKET
cat > "$stage/usr/lib/systemd/system/double-tap-switcher-mint@.service" <<'SERVICE'
[Unit]
Description=Double Tap Switcher Mint experimental input worker
[Service]
ExecStart=/usr/lib/double-tap-switcher-mint/double-tap-reader-mint
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
[ "$ID" = linuxmint ] && [ "$VERSION_ID" = 22.3 ] || { echo 'This test build requires Linux Mint 22.3.' >&2; exit 1; }
PRE
cat > "$stage/DEBIAN/postinst" <<'POST'
#!/bin/sh
set -e
if [ "$1" = configure ]; then
 systemctl daemon-reload
 systemctl enable --now double-tap-switcher-mint.socket
fi
POST
cat > "$stage/DEBIAN/prerm" <<'PRE'
#!/bin/sh
set -e
if [ "$1" = remove ] || [ "$1" = deconfigure ]; then
 systemctl disable --now double-tap-switcher-mint.socket || true
 systemctl stop 'double-tap-switcher-mint@*.service' || true
fi
PRE
chmod 755 "$stage/DEBIAN/"{preinst,postinst,prerm}
install -m644 "$base/LICENSE" "$stage/usr/share/doc/double-tap-switcher-mint/copyright"
install -m644 "$base/mint_native/README.md" "$stage/usr/share/doc/double-tap-switcher-mint/"
dpkg-deb --root-owner-group --build "$stage" "$out/double-tap-switcher-mint_1.38_amd64.deb"
(cd "$out" && sha256sum double-tap-switcher-mint_1.38_amd64.deb > SHA256SUMS)
