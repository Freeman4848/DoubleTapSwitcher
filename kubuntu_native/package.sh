#!/usr/bin/env bash
set -euo pipefail
base=$(cd -- "$(dirname -- "$0")/.." && pwd)
out="$base/kubuntu_native/dist"; mkdir -p "$out"
stage=$(mktemp -d); trap 'rm -rf -- "$stage"' EXIT
lib="$stage/usr/lib/double-tap-switcher-kubuntu"
mkdir -p "$stage/DEBIAN" "$lib/icons" "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/lib/systemd/system" "$stage/usr/share/doc/double-tap-switcher-kubuntu"
cat > "$stage/DEBIAN/control" <<'CONTROL'
Package: double-tap-switcher-kubuntu
Version: 1.37
Architecture: amd64
Maintainer: Freeman4848
Depends: libc6 (>= 2.38), libstdc++6 (>= 13), libgcc-s1, libsystemd0, libx11-6, libxtst6, libqt6core6t64, libqt6gui6, libqt6widgets6, libqt6dbus6, libqt6network6, qt6-qpa-plugins, libqt6waylandclient6, libatspi2.0-0t64, systemd
Conflicts: double-tap-switcher
Section: utils
Priority: optional
Description: Experimental Double Tap Switcher for Kubuntu KDE X11
 Test build only. GUI password fields are not detected. Not a production release.
CONTROL
install -m755 "$base/kubuntu_native/build/double-tap-switcher-kubuntu" "$base/kubuntu_native/build/double-tap-reader-kubuntu" "$lib/"
install -m644 "$base/kubuntu_native/build/icons/"*.png "$lib/icons/"
cat > "$stage/usr/bin/double-tap-switcher-kubuntu" <<'LAUNCH'
#!/bin/sh
case "${XDG_SESSION_TYPE:-wayland}" in
 x11) export QT_QPA_PLATFORM=xcb ;;
 *) export QT_QPA_PLATFORM=wayland ;;
esac
exec /usr/lib/double-tap-switcher-kubuntu/double-tap-switcher-kubuntu "$@"
LAUNCH
chmod 755 "$stage/usr/bin/double-tap-switcher-kubuntu"
cat > "$stage/usr/share/applications/double-tap-switcher-kubuntu.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Double Tap Switcher (Kubuntu TEST)
Comment=Experimental KDE/Wayland build; not certified for password fields
Exec=/usr/bin/double-tap-switcher-kubuntu
Icon=/usr/lib/double-tap-switcher-kubuntu/icons/dt-app.png
Terminal=false
Categories=Utility;Accessibility;
DESKTOP
cat > "$stage/usr/lib/systemd/system/double-tap-switcher-kubuntu.socket" <<'SOCKET'
[Unit]
Description=Double Tap Switcher Kubuntu test socket
[Socket]
ListenStream=/run/double-tap-switcher-kubuntu.sock
SocketMode=0666
Accept=yes
RemoveOnStop=yes
[Install]
WantedBy=sockets.target
SOCKET
cat > "$stage/usr/lib/systemd/system/double-tap-switcher-kubuntu@.service" <<'SERVICE'
[Unit]
Description=Double Tap Switcher Kubuntu experimental input worker
[Service]
ExecStart=/usr/lib/double-tap-switcher-kubuntu/double-tap-reader-kubuntu
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
[ "$ID" = ubuntu ] && [ "$VERSION_ID" = 26.04 ] || { echo 'This test build requires Kubuntu 26.04.' >&2; exit 1; }
PRE
cat > "$stage/DEBIAN/postinst" <<'POST'
#!/bin/sh
set -e
if [ "$1" = configure ]; then
 systemctl daemon-reload
 systemctl enable --now double-tap-switcher-kubuntu.socket
fi
POST
cat > "$stage/DEBIAN/prerm" <<'PRE'
#!/bin/sh
set -e
if [ "$1" = remove ] || [ "$1" = deconfigure ]; then
 systemctl disable --now double-tap-switcher-kubuntu.socket || true
 systemctl stop 'double-tap-switcher-kubuntu@*.service' || true
fi
PRE
chmod 755 "$stage/DEBIAN/"{preinst,postinst,prerm}
install -m644 "$base/LICENSE" "$stage/usr/share/doc/double-tap-switcher-kubuntu/copyright"
install -m644 "$base/kubuntu_native/README.md" "$stage/usr/share/doc/double-tap-switcher-kubuntu/"
dpkg-deb --root-owner-group --build "$stage" "$out/double-tap-switcher-kubuntu_1.37_amd64.deb"
(cd "$out" && sha256sum double-tap-switcher-kubuntu_1.37_amd64.deb > SHA256SUMS)
